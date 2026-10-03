#include <cassert>
#include <iostream>
#include <thread>
#include "../core/panel_controller.h"
#include "sample_card.h"
using namespace forge;

void MailboxOwnership() {
    InspectorMailbox m; InspectorAudio out;
    assert(!m.Read(out) && !m.AudioBegin());
    m.RequestRefresh(); auto* a=m.AudioBegin(); assert(a);
    a->block=123; m.RequestRefresh(); // cannot request while audio is writing
    assert(!m.AudioBegin() && !m.Read(out));
    m.AudioEnd(); m.RequestRefresh(); assert(!m.AudioBegin());
    assert(m.Read(out) && out.block==123 && !m.Read(out));
    // A slow main reader must see one coherent publication even under concurrency.
    std::thread audio([&] { for(unsigned i=1;i<=10000;) if(auto* state=m.AudioBegin()) {
        state->block=i; state->time_ms=i; state->raw_turns[0]=i; m.AudioEnd(); ++i;
    }});
    for(unsigned i=1;i<=10000;++i) {
        m.RequestRefresh(); while(!m.Read(out)) std::this_thread::yield();
        assert(out.block==i && out.time_ms==i && out.raw_turns[0]==i);
    }
    audio.join();
}
void WireBoundsAndLog() {
    InspectorSnapshot s; InspectorLog log;
    for(unsigned i=0;i<70;++i) log.Add({0,i,i,InspectorEventKind::Knob,0});
    InspectorEvent batch[3]; assert(log.After(0,batch)==3 && batch[0].serial==7 && log.Overwritten()==6);
    assert(log.After(70,batch)==0 && log.After(69,batch)==1 && batch[0].serial==70);
    s.audio.physical_keys=(uint64_t(1)<<39)|1; s.audio.raw_turns[0]=0xffffffffu;
    s.system.uptime_ms=0xffffffffu; s.generation=0xffffffffu;
    uint8_t out[kMaxReply+16];
    for(unsigned version=1;version<=4;++version) for(uint8_t page=2;page<=7;++page) {
        s.audio.patch.version=version;
        std::fill(out,out+sizeof(out),0xa5);
        const size_t n=EncodeInspector(16383,page,s,log,0,out);
        assert(n<=kMaxReply && Checksum(out,n)==0);
        for(size_t i=0;i<n;++i) assert(out[i]<128);
        for(size_t i=n;i<sizeof(out);++i) assert(out[i]==0xa5);
    }
    uint8_t request[14]; Header(request,0x0b,1); request[7]=6;
    InspectorWord(request+8,0xffffffffu); request[13]=Checksum(request,13);
    Request r; assert(DecodeRequest(request,14,r)==Error::None && r.inspector_cursor==0xffffffffu);
    request[12]=16; request[13]=Checksum(request,13); assert(DecodeRequest(request,14,r)==Error::Patch);
    request[7]=3; request[12]=0; request[13]=Checksum(request,13); assert(DecodeRequest(request,14,r)==Error::Length);
}
struct Sink : PanelSink {
    bool PresetAction(const MenuAction&,const Parameters&) override {return true;}
    bool SampleJob(const forge::SampleJob&) override {return true;}
    void Flash(bool) override {}
};
void PhysicalVersusInjectedAndVoiceEdges() {
    float l[48002]{},r[48002]{}; Engine e; assert(e.Init(48000,l,r,48002));
    Parameters patch; patch.version=4; patch.synth=true; assert(e.ApplyPatch(patch));
    int16_t memory[960]{}; SampleSlot slot; Recorder rec; rec.Init(memory,480,&slot,48000);
    PanelController panel; Sink sink; InspectorAudio a;
    SpscQueue<InspectorEvent,64> events; std::atomic<uint32_t> drops{0};
    panel.SetInspectorEvents(&events,&drops,100);
    PanelInput physical; physical.keys=uint64_t(1)<<15; physical.turns[3]=-1;
    panel.Inject({PanelEvent::Kind::Key,18,1}); panel.Inject({PanelEvent::Kind::Turn,3,2});
    panel.Block(physical,e,rec,sink); panel.Inspect(a); e.Inspect(a);
    assert(a.physical_keys==(uint64_t(1)<<15) && a.logical_keys==((uint64_t(1)<<15)|(uint64_t(1)<<18)));
    assert(a.raw_turns[3]==0xffffffffu && a.turns[3]==1 && a.logical_flags&8);
    e.ObserveVoiceEdges(events,drops,100); unsigned starts=0,keys=0,knobs=0;
    InspectorEvent event;
    while(events.Pop(event)) { if(event.kind==InspectorEventKind::VoiceStart) ++starts;
        if(event.kind==InspectorEventKind::KeyDown) {++keys; assert(event.value==1 || event.value==2);}
        if(event.kind==InspectorEventKind::Knob) {++knobs; assert(event.value==1);}
    }
    assert(starts==2 && keys==2 && knobs==1);
    e.Panic(); e.ObserveVoiceEdges(events,drops,101); unsigned stops=0;
    while(events.Pop(event)) if(event.kind==InspectorEventKind::VoiceStop) ++stops;
    assert(stops==2 && !drops.load());
    physical.keys=0; physical.turns[3]=0; panel.Inject({PanelEvent::Kind::Release,0,0});
    panel.Block(physical,e,rec,sink);
    unsigned ups=0;
    while(events.Pop(event)) if(event.kind==InspectorEventKind::KeyUp) {
        ++ups; assert((event.id==15 && event.value==1) || (event.id==18 && event.value==2));
    }
    assert(ups==2);
    physical.keys=uint64_t(1)<<15; panel.Inject({PanelEvent::Kind::Key,18,1});
    panel.Block(physical,e,rec,sink); while(events.Pop(event)) {}
    // Overflow never stalls or replaces unread records; losses remain visible.
    for(unsigned i=0;i<63;++i) assert(events.Push({}));
    physical.keys=0; physical.turns[3]=0; panel.Inject({PanelEvent::Kind::Release,0,0});
    panel.Block(physical,e,rec,sink); assert(drops.load()==2);
}
void FailedLoadIsVisible() {
    SampleTable table; SampleHandoff handoff; SampleLoader loader; SampleCard files;
    int16_t pool[1024]{}; uint8_t scratch[512]{};
    loader.Init(&table,&handoff,pool,1024,scratch,512);
    files.files["jammi_a1.wav"]={0,1,2};
    Engine e; float l[48002]{},r[48002]{}; assert(e.Init(48000,l,r,48002));
    Parameters p; p.version=4;p.synth=true;p.source=1;
    SampleEvent event;
    for(unsigned i=0;i<16;++i) { e.SetSampleFilesAvailable(handoff.AudioBlock(e)); loader.Poll(files,PackSelection(p),nullptr,event); }
    InspectorStorage st; loader.Inspect(st);
    assert(st.errors==1 && st.last_error==8 && st.file_loaded_frames==0 && st.pool_capacity_bytes==2048);
}
int main() { MailboxOwnership(); WireBoundsAndLog(); PhysicalVersusInjectedAndVoiceEdges(); FailedLoadIsVisible();
    std::cout<<"PASS: Inspector mailbox concurrency, bounded wire pages, event cursor/overflow, physical input identity, voice edges, failed loads\n";
}
