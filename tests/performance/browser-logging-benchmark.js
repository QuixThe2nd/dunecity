'use strict';

// Test-only shell instrumentation. Loaded before the unmodified shipped runtime.
// All samples stay in RAM until the timed interval has ended. No per-frame I/O.
const benchmark = {frames:[], longTasks:[], syncs:[], visibility:[], done:false,
    yieldCount:0,yieldStacks:[],
    savePath:'/home/web_user/.config/DuneCity/save/benchmark-city.dls'};
Module.preRun = [function() {
    FS.mkdirTree('/home/web_user');
    FS.mount(IDBFS, {}, '/home/web_user');
    addRunDependency('benchmark-fixture');
    FS.syncfs(true, async function(error) {
        if (error) throw error;
        if (FS.readdir('/home/web_user').some(x=>x!=='.' && x!=='..'))
            throw new Error('Benchmark origin is not fresh; choose a new port.');
        const fixture = await (await fetch('/fixture.json')).json();
        benchmark.fixture=fixture;
        for (const [name,data] of Object.entries(fixture.files)) {
            const path='/home/web_user/.config/DuneCity/'+name;
            FS.mkdirTree(path.slice(0,path.lastIndexOf('/')));
            FS.writeFile(path,Uint8Array.from(atob(data),x=>x.charCodeAt(0)));
        }
        FS.mkdirTree('/home/web_user/.config/DuneCity/save');
        FS.writeFile(benchmark.savePath,new Uint8Array(await (await fetch('/fixture-save')).arrayBuffer()));
        removeRunDependency('benchmark-fixture');
    });
}];

Module.onRuntimeInitialized = function() {
    const arm=document.createElement('button');
    arm.className='tool';arm.textContent='Arm benchmark';
    arm.onclick=()=>{benchmark.armed=true;arm.disabled=true;arm.textContent='Load the saved game';};
    document.querySelector('.toolbar').appendChild(arm);
    const originalOpen=FS.open;
    FS.open=function(path,...args) {
        // The home menu also reads save headers for Continue. Arm explicitly
        // after startup so those metadata reads cannot start the measurement.
        if (benchmark.armed && path===benchmark.savePath) benchmark.loadSeen=true;
        return originalOpen.call(this,path,...args);
    };
    const originalSync=FS.syncfs;
    FS.syncfs=function(populate,callback) {
        const sample={start:performance.now(),populate};
        const result=originalSync.call(this,populate,function(error) {
            sample.end=performance.now();sample.error=error?String(error):null;
            callback(error);
        });
        sample.invocation_ms=performance.now()-sample.start;
        benchmark.syncs.push(sample);
        return result;
    };
    document.addEventListener('visibilitychange',()=>benchmark.visibility.push({at:performance.now(),state:document.visibilityState}));
    benchmark.initialVisibility=document.visibilityState;
    const observer=new PerformanceObserver(list=>{
        if (!benchmark.done) for(const entry of list.getEntries())
            benchmark.longTasks.push({start:entry.startTime,duration:entry.duration});
    });
    observer.observe({type:'longtask',buffered:true});
    const originalSleep=Asyncify.handleSleep;
    Asyncify.handleSleep=function(startAsync) {
        // SDL 2.32.8 yields inside Emscripten_GLES_SwapWindow, then the game
        // yields again at loop end. Sample every other normal-state call to
        // measure presentation-to-presentation, ignoring all rewind calls.
        if (benchmark.loadSeen && !benchmark.done && Asyncify.state===Asyncify.State.Normal) {
            const ordinal=benchmark.yieldCount++;
            if (ordinal<8) benchmark.yieldStacks.push(new Error().stack);
            if (ordinal%2) return originalSleep.call(this,startAsync);
            const now=performance.now();
            if (benchmark.started===undefined) {
                benchmark.started=now;
                benchmark.measureAfter=now+benchmark.fixture.warmup*1000;
                benchmark.stopAfter=benchmark.measureAfter+benchmark.fixture.seconds*1000;
                benchmark.visibility.push({at:now,state:document.visibilityState,focused:document.hasFocus()});
                console.info('BROWSER_BENCHMARK_STARTED',benchmark.fixture.logging);
            }
            if (benchmark.previous>=benchmark.measureAfter)
                benchmark.frames.push({start:benchmark.previous,duration:now-benchmark.previous});
            benchmark.previous=now;
            if (now>=benchmark.stopAfter) {
                benchmark.done=true;
                // Reporting and log inspection happen after measurement.
                setTimeout(()=>finishBenchmark(observer),0);
            }
        }
        return originalSleep.call(this,startAsync);
    };
};

async function finishBenchmark(observer) {
    observer.disconnect();
    const frames=benchmark.frames, sorted=frames.map(f=>f.duration).sort((a,b)=>a-b);
    const total=sorted.reduce((a,b)=>a+b,0);
    const percentile=q=>sorted[Math.floor((sorted.length-1)*q)];
    const files=[];
    function walk(path) {
        for(const name of FS.readdir(path)) {
            if(name==='.' || name==='..')continue;
            const p=path+'/'+name,stat=FS.stat(p);
            if(FS.isDir(stat.mode))walk(p);else files.push({path:p,bytes:stat.size});
        }
    }
    walk('/home/web_user');
    const performanceWindows=[];
    for(const file of files.filter(f=>f.path.endsWith('/events.jsonl'))) {
        for(const line of FS.readFile(file.path,{encoding:'utf8'}).split('\n')) {
            if(line.includes('"event":"performance_window"')) {
                try { const row=JSON.parse(line); performanceWindows.push(row.data); } catch (_) {}
            }
        }
    }
    const summary={frames:frames.length,elapsed_ms:total,fps:frames.length*1000/total,
        median_ms:percentile(.5),p95_ms:percentile(.95),p99_ms:percentile(.99),max_ms:sorted.at(-1)};
    for(const ms of [50,100,250,500,1000])summary['over_'+ms+'ms']=sorted.filter(x=>x>ms).length;
    const result={run:benchmark.fixture.run,logging:benchmark.fixture.logging,
        save_sha256:benchmark.fixture.save_sha256,summary,frames,
        measurement_start:frames[0].start,measurement_end:benchmark.previous,
        syncs:benchmark.syncs,visibility:benchmark.visibility,initialVisibility:benchmark.initialVisibility,
        yieldCount:benchmark.yieldCount,yieldStacks:benchmark.yieldStacks,
        longTasks:benchmark.longTasks.filter(t=>t.start>=frames[0].start && t.start<benchmark.previous),
        userAgent:navigator.userAgent,canvas:{width:canvas.width,height:canvas.height},files,performanceWindows};
    benchmark.result=result;
    const response=await fetch('/result',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(result)});
    if(!response.ok)throw new Error('Could not store benchmark result');
    benchmark.saved=true;
    document.title='Benchmark complete: '+benchmark.fixture.logging;
    console.info('BROWSER_BENCHMARK_COMPLETE',JSON.stringify(summary));
}
