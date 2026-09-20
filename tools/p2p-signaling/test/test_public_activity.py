"""Real HTTP coverage for public records, mod discovery and waiting presence."""
import json
from pathlib import Path
import unittest
from test_signaling import SignalingTestCase, claims

class PublicActivityTests(SignalingTestCase):
    def setUp(self):
        super().setUp()
        Path(self.service.activity_log).write_text('')

    def events(self):
        return [json.loads(s) for s in Path(self.service.activity_log).read_text().splitlines()]

    def chat(self, action, **fields):
        return self.service.request('POST','/v1/lobby/'+action,dict(claims(),**fields))

    def test_named_public_lifecycle_and_start_roster(self):
        admission, host=self.seat('Host',visibility='public')
        guest=self.seat_guest(admission,'Guest',runtime='browser')
        self.assertEqual(403,self.phase(guest.fields['session'],'match').status)
        for _ in range(2): self.assertEqual(200,self.phase(host.fields['session'],'match').status)
        events=self.events()
        self.assertEqual(['public_game_created','public_game_joined','public_game_started'],[e['kind'] for e in events])
        self.assertEqual(['Host','Guest','Host'],[e['player_name'] for e in events])
        self.assertEqual(['Host','Guest'],[p['name'] for p in events[-1]['players']])
        self.assertEqual(['native','browser'],[p['runtime'] for p in events[-1]['players']])
        for secret in (admission.fields['room'],admission.fields['grant'],host.fields['session']):
            self.assertNotIn(secret,json.dumps(events))

    def test_private_rooms_do_not_create_named_public_records(self):
        admission, host=self.seat('Private Host',visibility='private')
        self.seat_guest(admission,'Private Guest')
        self.assertEqual(200,self.phase(host.fields['session'],'match').status)
        self.assertEqual([],self.events())

    def test_only_accepted_chat_uses_session_name(self):
        session=self.chat('enter',name='Duncan'.encode().hex()).fields['session']
        self.assertEqual(200,self.chat('say',session=session,name='Forged'.encode().hex(),text='hello 世界'.encode().hex()).status)
        self.assertEqual(403,self.chat('say',session='0'*64,text='refused'.encode().hex()).status)
        self.assertEqual(200,self.chat('poll',session=session,cursor=0).status)
        e=self.events(); self.assertEqual(1,len(e))
        self.assertEqual(('Duncan','hello 世界'),(e[0]['player_name'],e[0]['message']))
        self.assertNotIn(session,json.dumps(e))

    def test_server_analytics_setting_disables_capture_but_not_presence(self):
        self.service.write_config(analytics_enabled=False)
        try:
            session=self.chat('enter',name='Duncan'.encode().hex()).fields['session']
            self.assertEqual(200,self.chat('say',session=session,text='hello'.encode().hex()).status)
            self.assertEqual('1',self.chat('poll',session=session,cursor=0,presence=1).fields['online'])
            self.assertEqual([],self.events())
        finally: self.service.write_config()

    def test_storage_failure_does_not_block_public_chat(self):
        marker=Path(self.service.tmp)/'fail-public-activity'; marker.touch()
        try:
            session=self.chat('enter',name='Duncan'.encode().hex()).fields['session']
            self.assertEqual(200,self.chat('say',session=session,text='hello'.encode().hex()).status)
        finally: marker.unlink()

    def test_presence_across_mods_expires_without_expiring_chat_identity(self):
        first=self.chat('enter',name='Alice'.encode().hex()).fields['session']
        second=self.service.request('POST','/v1/lobby/enter',claims(contentHash='b'*64,name='Bob'.encode().hex())).fields['session']
        seen=self.chat('poll',session=first,cursor=0,presence=1)
        self.assertEqual('2',seen.fields['online'])
        self.assertEqual(['Alice','Bob'],[bytes.fromhex(x).decode() for x in seen.multi['waiting']])
        state_path=Path(self.service.state)/'lobby.json';state=json.loads(state_path.read_text())
        state['sessions'][second]['lastSeen']-=21000; state_path.write_text(json.dumps(state))
        self.assertEqual('1',self.chat('poll',session=first,cursor=0,presence=1).fields['online'])
        self.assertNotIn('online',self.chat('poll',session=first,cursor=0).fields)
        self.assertEqual([],self.events())

    def test_all_mods_discovery_keeps_admission_content_checks(self):
        a=self.host(visibility='public'); self.session(a.fields['grant'],'First')
        b=self.service.request('POST','/v1/admission/host',claims(contentHash='b'*64,mod='dunecity'.encode().hex(),mode='custom',maxPeers=4,visibility='public'))
        self.assertEqual(200,self.session(b.fields['grant'],'Other mod',contentHash='b'*64).status)
        legacy=self.service.request('POST','/v1/admission/list',claims(offset=0))
        self.assertEqual(1,len(legacy.multi['game'])); self.assertEqual(5,len(legacy.multi['game'][0].split('|')))
        allmods=self.service.request('POST','/v1/admission/list',claims(offset=0,allMods=1))
        self.assertEqual(2,len(allmods.multi['game']))
        self.assertTrue(all(len(g.split('|'))==7 for g in allmods.multi['game']))
        row=next(g.split('|') for g in allmods.multi['game'] if g.startswith(b.fields['room']))
        self.assertEqual(('b'*64,'dunecity'),(row[5],bytes.fromhex(row[6]).decode()))
        self.assertEqual(409,self.join(b.fields['room']).status)

if __name__=='__main__': unittest.main()
