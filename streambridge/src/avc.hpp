#pragma once
#include "mux.hpp"
namespace transport {
// Remove only user_data_unregistered SEI (encoder identification), retaining
// buffering/picture timing and all other SEI messages. Input is Annex B.
inline Bytes avc_for_transport(const Bytes &input){
    std::vector<std::pair<size_t,size_t>> starts;
    for(size_t i=0;i+3<input.size();){
        size_t n=0;
        if(input[i]==0&&input[i+1]==0){
            if(input[i+2]==1)n=3;
            else if(input[i+2]==0&&input[i+3]==1)n=4;
        }
        if(n){starts.emplace_back(i,i+n);i+=n;}else ++i;
    }
    Bytes out;
    for(size_t i=0;i<starts.size();++i){
        size_t begin=starts[i].first,nal=starts[i].second,end=i+1<starts.size()?starts[i+1].first:input.size();
        if(nal>=end)throw std::runtime_error("Lege AVC-NAL.");
        if((input[nal]&31)!=6){out.insert(out.end(),input.begin()+begin,input.begin()+end);continue;}
        Bytes rbsp;int zeros=0;
        for(size_t j=nal+1;j<end;++j){
            uint8_t b=input[j];if(zeros==2&&b==3){zeros=0;continue;}
            rbsp.push_back(b);zeros=b==0?zeros+1:0;
        }
        Bytes kept;size_t pos=0;
        while(pos<rbsp.size()&&!(rbsp[pos]==0x80&&pos+1==rbsp.size())){
            size_t start=pos,type=0,size=0;
            while(pos<rbsp.size()&&rbsp[pos]==255){type+=255;++pos;}
            if(pos>=rbsp.size())throw std::runtime_error("Ongeldige AVC-SEI-type.");
            type+=rbsp[pos++];
            while(pos<rbsp.size()&&rbsp[pos]==255){size+=255;++pos;}
            if(pos>=rbsp.size())throw std::runtime_error("Ongeldige AVC-SEI-lengte.");
            size+=rbsp[pos++];
            if(size>rbsp.size()-pos)throw std::runtime_error("Onvolledige AVC-SEI.");
            pos+=size;if(type!=5)kept.insert(kept.end(),rbsp.begin()+start,rbsp.begin()+pos);
        }
        if(kept.empty())continue;
        kept.push_back(0x80);out.insert(out.end(),{0,0,0,1,6});zeros=0;
        for(uint8_t b:kept){if(zeros==2&&b<=3){out.push_back(3);zeros=0;}out.push_back(b);zeros=b==0?zeros+1:0;}
    }
    if(out.empty())throw std::runtime_error("AVC-access-unit zonder NAL-data.");
    return out;
}
inline size_t first_slice(const Bytes &b){
    for(size_t i=0;i+3<b.size();++i){
        if(b[i]==0&&b[i+1]==0&&b[i+2]==1){
            int type=b[i+3]&31;if(type==1||type==5)return i+3;
        }
    }
    throw std::runtime_error("AVC-access-unit zonder slice.");
}
}
