"""Late-join requests confer no membership until the host approves a bounded grant."""
import json
from pathlib import Path
import unittest
from test_signaling import SignalingTestCase, claims

class LateJoinTests(SignalingTestCase):
    def running(self, **fields):
        a=self.service.request('POST','/v1/admission/host',claims(visibility='public',maxPeers=4,mode='custom',allowLateJoin=1,map='Test map'.encode().hex(),**fields))
        h=self.session(a.fields['grant'],'Host')
        self.assertEqual(200,self.phase(h.fields['session'],'match').status)
        return a,h
    def request(self,a,name='Guest',**fields):
        return self.service.request('POST','/v1/admission/request',claims(room=a.fields['room'],name=name.encode().hex(),publicOnly=1,**fields))
    def status(self,ticket,**fields):
        return self.service.request('POST','/v1/admission/request-status',claims(request=ticket,**fields))
    def manage(self,h,action='list',request=''):
        return self.service.request('POST','/v1/p2p/join-requests',dict(action=action,**({'request':request} if request else {})),headers={'X-Dune-Session':h.fields['session']})
    def test_running_directory_is_opt_in_and_carries_map_time(self):
        a,h=self.running()
        page=self.service.request('POST','/v1/admission/list',claims(offset=0,allMods=1,details=1))
        row=page.multi['game'][0].split('|')
        self.assertEqual(11,len(row)); self.assertEqual('Test map',bytes.fromhex(row[7]).decode())
        self.assertEqual(['match','0','1'],row[8:])
        self.assertNotIn('game',self.service.request('POST','/v1/admission/list',claims(offset=0,allMods=1)).fields)
        self.assertEqual(409,self.join(a.fields['room']).status)
    def test_host_approval_issues_name_bound_grant_and_does_not_reopen_general_admission(self):
        a,h=self.running(); r=self.request(a); self.assertEqual(200,r.status)
        ticket=r.fields['request']; self.assertEqual('pending',self.status(ticket).fields['requestState'])
        queue=self.manage(h); self.assertEqual(200,queue.status)
        request=queue.multi['request'][0].split('|')[0]
        self.assertEqual(200,self.manage(h,'approve',request).status)
        approved=self.status(ticket); self.assertEqual('approved',approved.fields['requestState'])
        self.assertEqual(409,self.join(a.fields['room']).status)
        g=self.session(approved.fields['grant'],'Guest'); self.assertEqual(200,g.status)
        self.assertEqual(403,self.manage(g).status)
        self.assertEqual('joined',self.status(ticket).fields['requestState'])
        self.assertEqual(200,self.phase(h.fields['session'],'match').status)
    def test_a_wrong_name_cannot_redeem_an_approved_grant(self):
        a,h=self.running(); r=self.request(a); request=self.manage(h).multi['request'][0].split('|')[0]
        self.manage(h,'approve',request)
        self.assertNotEqual(200,self.session(self.status(r.fields['request']).fields['grant'],'Other name').status)
    def test_decline_cancel_and_mismatching_claims(self):
        a,h=self.running(); r=self.request(a)
        self.assertEqual(409,self.request(a,contentHash='b'*64).status)
        self.assertEqual(403,self.status(r.fields['request'],runtime='browser').status)
        self.assertEqual('cancelled',self.status(r.fields['request'],cancel=1).fields['requestState'])
        r=self.request(a,'Another'); request=self.manage(h).multi['request'][0].split('|')[0]
        self.manage(h,'decline',request)
        self.assertEqual('declined',self.status(r.fields['request']).fields['requestState'])
    def test_waiting_polls_do_not_spend_the_small_admission_allowance(self):
        a,h=self.running(); r=self.request(a)
        for _ in range(30): self.assertEqual(200,self.status(r.fields["request"]).status)
    def test_abort_before_approval_prevents_a_racing_approval(self):
        a,h=self.running(); r=self.request(a); request=self.manage(h).multi["request"][0].split("|")[0]
        self.assertEqual(200,self.manage(h,"abort",request).status)
        self.assertEqual(409,self.manage(h,"approve",request).status)
        self.assertEqual("declined",self.status(r.fields["request"]).fields["requestState"])
    def test_disabled_legacy_room_does_not_accept_requests(self):
        a,h=self.seat('Host',visibility='public'); self.phase(h.fields['session'],'match')
        self.assertEqual(409,self.request(a).status)
    def test_abort_removes_only_newcomer_and_preserves_running_roster(self):
        a,h=self.running(); r=self.request(a); request=self.manage(h).multi['request'][0].split('|')[0]
        self.manage(h,'approve',request); guest=self.session(self.status(r.fields['request']).fields['grant'],'Guest')
        self.assertEqual(200,guest.status); self.assertEqual(200,self.manage(h,'abort',request).status)
        self.assertEqual(200,self.service.request('POST','/v1/p2p/poll',dict(cursor=0),headers={'X-Dune-Session':h.fields['session']}).status)
        self.assertNotEqual(200,self.service.request('POST','/v1/p2p/poll',dict(cursor=0),headers={'X-Dune-Session':guest.fields['session']}).status)

if __name__=='__main__': unittest.main()
