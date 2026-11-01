#include "Tek2221Native.h"
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <cmath>
using namespace Tek2221;
static void Check(bool b,const std::string& msg) { if(!b) throw std::runtime_error(msg); }
template<class F> static void Reject(F f)
{
    bool rejected=false; try { f(); } catch(const std::exception&) { rejected=true; }
    Check(rejected,"Expected explicit rejection");
}
static std::vector<uint8_t> Bytes(const std::string& path)
{
    std::ifstream f(path,std::ios::binary); Check(bool(f),"Cannot read "+path);
    return {std::istreambuf_iterator<char>(f),std::istreambuf_iterator<char>()};
}
struct Fake : MessageIO
{
    std::vector<std::string> commands;
    std::string preamble, identity="ID TEK/2221,V81.1,VERS:15;", failWrite, failRead;
    std::vector<uint8_t> raw;
    const std::string original="DATA SOURCE:ACQ,TARGET:REF4,CHANNEL:CH2,ENCDG:ASCII";
    void Write(const std::string& c) override
    {
        commands.push_back(c);
        if(c==failWrite) throw std::runtime_error("Injected write failure");
    }
    std::vector<uint8_t> ReadMessage() override
    {
        auto c=commands.back(); std::string reply;
        if(c==failRead) throw std::runtime_error("Injected read failure");
        if(c=="ID?") reply=identity;
        else if(c=="DATA?") reply=original+";";
        else if(c=="WFMPRE?") reply=preamble;
        else if(c=="CURVE?") return raw;
        else throw std::runtime_error("Unexpected query: "+c);
        return {reply.begin(),reply.end()};
    }
};
struct TwoChannels : Fake
{
    std::string preambles[2];
    std::vector<uint8_t> curves[2];
    unsigned selected=0;
    void Write(const std::string& c) override
    {
        Fake::Write(c);
        if(c=="DATA SOURCE:ACQ,CHANNEL:CH1,ENCDG:BINARY") selected=0;
        if(c=="DATA SOURCE:ACQ,CHANNEL:CH2,ENCDG:BINARY") selected=1;
    }
    std::vector<uint8_t> ReadMessage() override
    {
        preamble=preambles[selected]; raw=curves[selected];
        return Fake::ReadMessage();
    }
};
int main(int argc,char** argv)
{
    try
    {
        Check(argc==2,"Pass fixture root");
        for(std::string name:{"y8","y16"})
        {
            std::string path=std::string(argv[1])+"/"+name;
            auto text=Bytes(path+"/preamble.txt"); auto raw=Bytes(path+"/curve-response.bin");
            std::string pre(text.begin(),text.end()); auto r=Decode(pre,raw);
            Check(r.samples.size()==(name=="y8"?4096u:2048u),"Point count");
            Check(r.interval==(name=="y8"?5000000000LL:4000000000LL),"Sample time");
            Check(r.phase==-512*r.interval,"Trigger phase");
            auto mm=std::minmax_element(r.samples.begin(),r.samples.end());
            Check(std::abs((*mm.second-*mm.first)-(name=="y8"?.0544:.051546875))<1e-7,"P-p voltage/endianness");
            Fake f; f.preamble=pre; f.raw=raw;
            auto actual=CaptureCH1(f);
            Check(actual.samples==r.samples,"Capture/decoder mismatch");
            Check(f.commands==std::vector<std::string>{"ID?","DATA?","DATA SOURCE:ACQ,CHANNEL:CH1,ENCDG:BINARY","WFMPRE?","CURVE?",f.original},"Exact legacy command sequence or DATA restore");
            Fake ch2; ch2.preamble=pre; ch2.raw=raw;
            auto label=ch2.preamble.find(", CH1,"); Check(label!=std::string::npos,"Missing channel label");
            ch2.preamble.replace(label,6,", CH2,");
            auto second=CaptureChannel(ch2,2);
            Check(second.channel=="CH2" && second.samples==r.samples && second.bits==r.bits,"CH2 decode mismatch");
            Check(ch2.commands[2]=="DATA SOURCE:ACQ,CHANNEL:CH2,ENCDG:BINARY" && ch2.commands.back()==ch2.original,"CH2 selection/restore");
            Fake mismatch; mismatch.preamble=pre; mismatch.raw=raw;
            Reject([&]{CaptureChannel(mismatch,2);});
            Check(mismatch.commands.back()==mismatch.original,"Channel mismatch did not restore DATA");
            Fake invalid;
            Reject([&]{CaptureChannel(invalid,0);});
            Reject([&]{CaptureChannel(invalid,3);});
            Check(invalid.commands.empty(),"Invalid channel touched the instrument");
            for(auto fail:{"select","preamble","curve","checksum","restore"})
            {
                Fake broken; broken.preamble=pre; broken.raw=raw;
                if(std::string(fail)=="select") broken.failWrite="DATA SOURCE:ACQ,CHANNEL:CH1,ENCDG:BINARY";
                if(std::string(fail)=="preamble") broken.failRead="WFMPRE?";
                if(std::string(fail)=="curve") broken.failRead="CURVE?";
                if(std::string(fail)=="checksum") { auto m=std::find(broken.raw.begin(),broken.raw.end(),'%')-broken.raw.begin(); broken.raw[m+3]^=1; }
                if(std::string(fail)=="restore") broken.failWrite=broken.original;
                Reject([&]{ CaptureCH1(broken); });
                Check(broken.commands.back()==broken.original,"Failed to attempt DATA restoration");
            }
            Fake wrong; wrong.identity="SOME OTHER INSTRUMENT";
            Reject([&]{ CaptureCH1(wrong); });
            Check(wrong.commands==std::vector<std::string>{"ID?"},"Wrong instrument received a setting command");
            for(auto entry:std::vector<std::pair<std::string,std::string>>{{"PT.FMT:Y","PT.FMT:ENV"},{"PT.FMT:Y","PT.FMT:XY"},{"YUNIT:V","YUNIT:DIVS"},{"PT.OFF:512","PT.OFF:-10000"}})
            {
                auto bad=pre; auto pos=bad.find(entry.first); Check(pos!=std::string::npos,"Test field missing");
                bad.replace(pos,entry.first.size(),entry.second); Reject([&]{Decode(bad,raw);});
            }
            auto shifted=pre; auto pos=shifted.find("YOFF:0"); shifted.replace(pos,6,"YOFF:10");
            auto yoff=Decode(shifted,raw);
            double expected=name=="y8"?.008:.000078125;
            Check(std::abs((r.samples[0]-yoff.samples[0])-expected)<1e-7,"YOFF mapping");
            std::cout<<"PASS "<<name<<": CH1/CH2 decode, channel selection/mismatch, scaling, time zero, restoration on success/failures, identity guard, unsupported metadata\n";
        }
        TwoChannels pair;
        for(unsigned i=0;i<2;i++)
        {
            auto path=std::string(argv[1])+(i?"/y16":"/y8");
            auto p=Bytes(path+"/preamble.txt"); pair.preambles[i]=std::string(p.begin(),p.end());
            pair.curves[i]=Bytes(path+"/curve-response.bin");
        }
        auto pos=pair.preambles[1].find(", CH1,"); pair.preambles[1].replace(pos,6,", CH2,");
        auto first=CaptureChannel(pair,1); auto second=CaptureChannel(pair,2);
        Check(first.samples.size()==4096 && second.samples.size()==2048 && first.bits==8 && second.bits==16,"Sequential captures shared sample state");
        Check(first.interval==5000000000LL && second.interval==4000000000LL && first.range!=second.range,"Channels shared metadata");
        Check(std::count(pair.commands.begin(),pair.commands.end(),pair.original)==2,"Both transfers must restore DATA");
        std::cout<<"PASS sequential CH1/CH2: independent record sizes, bit widths, timing/ranges, DATA restoration\n";
        return 0;
    }
    catch(const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<"\n"; return 1; }
}
