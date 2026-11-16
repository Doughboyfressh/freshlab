#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
namespace fresh {
struct Step { int action=0,pitch=0;float velocity=.85f; };
struct SequencePatch { std::array<Step,16> steps{};int length=16,division=4;double swing=0; };
struct SequenceEvent {bool trigger=false,reverse=false,throwDelay=false;int step=-1,pitch=0;float velocity=0;double gateBeats=.1;};
class Sequencer {
    int64_t lastKey=std::numeric_limits<int64_t>::min();
public:
    void reset(){lastKey=std::numeric_limits<int64_t>::min();}
    SequenceEvent tick(double beats,const SequencePatch& p){
        SequenceEvent e;if(!std::isfinite(beats)||beats<0)return e;
        const double grid=beats*std::clamp(p.division,1,8);int64_t absolute=int64_t(std::floor(grid+1e-9));
        double phase=grid-absolute;const double swing=std::clamp(p.swing,0.0,.45);
        if((absolute&1)&&phase<swing){--absolute;phase+=1;}
        const double duration=(absolute&1)?1-swing:1+swing;
        const double local=std::clamp((phase-((absolute&1)?swing:0))/duration,0.0,.999999999);
        const int step=int(absolute%std::clamp(p.length,1,16));const auto& s=p.steps[size_t(step)];
        const int repeats=s.action==2?2:(s.action==3?3:1);
        const int repeat=int(local*repeats);const int64_t key=absolute*4+repeat;
        if(key==lastKey)return e;
        lastKey=key;
        if(s.action==0)return e;
        e.trigger=true;e.step=step;e.pitch=s.pitch;e.velocity=std::clamp(s.velocity,0.0f,1.0f);
        e.reverse=s.action==4;e.throwDelay=s.action==5;e.gateBeats=duration/(p.division*repeats)*.72;return e;
    }
};
} // namespace fresh
