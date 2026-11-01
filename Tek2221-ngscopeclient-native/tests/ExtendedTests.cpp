#include "Tek2221Controls.h"
#include <iostream>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <algorithm>
#include <cmath>
using namespace Tek2221;
static void Check(bool b,const std::string& msg) { if(!b) throw std::runtime_error(msg); }
template<class F> static void Reject(F f) { bool rejected=false; try { f(); } catch(const std::exception&) { rejected=true; } Check(rejected,"Expected rejection"); }
static std::string Preamble(const std::string& format,int bits,const std::string& channel="CH1",const std::string& source="ACQ")
{
    return "WFMPRE WFID:\""+source+", "+(format=="XY"?"XY, 20.0mV, AC, ":channel+", ")+"50.0mV, DC, 0.5mS, TEST\",NR.PT:3,PT.OFF:1,PT.FMT:"+format+",XMULT:0.001,XOFF:2,XUNIT:S,XINCR:5.0E-6,YMULT:0.002,YOFF:1,YUNIT:V,ENCDG:BINARY,BN.FMT:RP,BYT/NR:"+(bits==8?"1":"2")+",BIT/NR:"+std::to_string(bits)+",CRVCHK:CHKSM0;";
}
static std::vector<uint8_t> Curve(const std::vector<int>& values,int bits)
{
    std::vector<uint8_t> b={'C','U','R','V','E',' ','%'};
    auto length=values.size()*(bits/8)+1;
    b.push_back(uint8_t(length>>8)); b.push_back(uint8_t(length));
    unsigned sum=b[7]+b[8];
    for(auto v:values)
    {
        if(bits==16) { b.push_back(uint8_t(v>>8)); sum+=uint8_t(v>>8); }
        b.push_back(uint8_t(v)); sum+=uint8_t(v);
    }
    b.push_back(uint8_t(-sum)); b.push_back(';'); return b;
}
struct PairIO : MessageIO
{
    std::vector<std::string> commands;
    std::vector<bool> saved={true,true,true,true}; size_t saveAt=0;
    int selected=1; bool xy=false,mismatch=false,wrongSource=false;
    void Write(const std::string& c) override { commands.push_back(c); if(c.find("DATA SOURCE:ACQ,CHANNEL:CH2,")==0) selected=2; if(c.find("DATA SOURCE:ACQ,CHANNEL:CH1,")==0) selected=1; }
    std::vector<uint8_t> ReadMessage() override
    {
        std::string reply,c=commands.back();
        if(c=="ACQ? SAVE") { reply=std::string("ACQUISITION SAVE:")+(saved.at(saveAt++)?"ON;":"OFF;"); }
        else if(c=="ID?") reply="ID TEK/2221,V81.1,VERS:15;";
        else if(c=="DATA?") reply="DATA SOURCE:ACQ,TARGET:REF4,CHANNEL:CH1,ENCDG:ASCII;";
        else if(c=="WFMPRE?") { reply=Preamble(xy?"XY":"Y",8,"CH"+std::to_string(selected),wrongSource?"REF4":"ACQ"); if(mismatch && selected==2) reply.replace(reply.find("XINCR:5.0E-6"),12,"XINCR:4.0E-6"); }
        else if(c=="CURVE?") return xy?Curve({140,130,141,131,142,132},8):Curve({130,131,132},8);
        else throw std::runtime_error("Unexpected query "+c);
        return {reply.begin(),reply.end()};
    }
};
struct ControlsIO : MessageIO
{
    std::string reply,command; std::vector<std::string> commands;
    void Write(const std::string& c) override { command=c; commands.push_back(c); }
    std::vector<uint8_t> ReadMessage() override { return {reply.begin(),reply.end()}; }
};
struct SetupIO : MessageIO
{
    std::map<std::string,std::string> settings;
    std::vector<std::string> writes;
    bool rejectSmooth=false,changeSweeps=false;
    SetupIO(const std::string& text) : settings(ParseSetup(text)) {}
    void Write(const std::string& command) override
    {
        writes.push_back(command);
        if(command=="SET?") return;
        Check(command.size()<80 && command.find(';')==std::string::npos,"Setup restore must use short separate commands");
        for(const auto& p:ParseSetup(command)) if(!(rejectSmooth && p.first=="ACQ SMO")) settings[p.first]=p.second;
    }
    std::vector<uint8_t> ReadMessage() override
    {
        Check(writes.back()=="SET?","Expected setup query");
        auto reply=BuildSetup(settings);
        return {reply.begin(),reply.end()};
    }
};
int main(int argc,char** argv)
{
    try
    {
        for(int bits:{8,16})
        {
            int z=bits==8?128:32768;
            auto env=Decode(Preamble("ENV",bits),Curve({z+8,z-8,z+10,z-4,z+20,z},bits));
            Check(env.samples.size()==3 && env.minimum.size()==3 && env.format=="ENV","Envelope shape");
            Check(std::abs(env.samples[0]-.014f)<1e-7 && std::abs(env.minimum[0]+.018f)<1e-7,"Envelope calibrated bounds/endianness");
            Reject([&]{Decode(Preamble("ENV",bits),Curve({z-8,z+8,z,z,z,z},bits));});
            auto xy=Decode(Preamble("XY",bits),Curve({z+8,z-8,z+10,z-4,z+20,z},bits));
            auto x=XYComponent(xy,1),y=XYComponent(xy,2);
            Check(std::abs(x.samples[0]-.006f)<1e-7 && std::abs(y.samples[0]+.018f)<1e-7,"XY independent X/Y calibration");
            Check(std::abs(x.range-.16)<1e-10 && std::abs(y.range-.4)<1e-10 && x.coupling=="AC" && y.coupling=="DC","XY axis metadata");
            Check(x.phase==-5000000000LL && x.interval==y.interval,"XY time zero");
            ValidatePair(x,y);
            auto ref=Decode(Preamble("Y",bits,"CH2","REF4"),Curve({z,z+5,z+10},bits));
            Check(ref.source=="REF4" && ref.channel=="CH2","REF4 identity");
            Reject([&]{ValidatePair(ref,y);});
            std::cout<<"PASS "<<bits<<"-bit ENV, XY, independent axis scaling, REF4, malformed bounds\n";
        }
        PairIO live; live.saved={false}; Reject([&]{CapturePair(live);});
        Check(live.commands==std::vector<std::string>{"ACQ? SAVE"},"Unsaved pair changed DATA");
        PairIO mid; mid.saved={true,false}; Reject([&]{CapturePair(mid);});
        Check(std::count(mid.commands.begin(),mid.commands.end(),"CURVE?")==1,"Pair continued after SAVE released");
        Check(std::find(mid.commands.begin(),mid.commands.end(),"DATA SOURCE:ACQ,TARGET:REF4,CHANNEL:CH1,ENCDG:ASCII")!=mid.commands.end(),"Pair failure did not restore DATA");
        PairIO end; end.saved={true,true,false}; Reject([&]{CapturePair(end);});
        PairIO mismatch; mismatch.mismatch=true; Reject([&]{CapturePair(mismatch);});
        PairIO pair; auto result=CapturePair(pair);
        Check(result.size()==2 && result[0].channel=="CH1" && result[1].channel=="CH2" && pair.saveAt==3,"Pair checks");
        PairIO xy; xy.xy=true; auto axes=CapturePair(xy);
        Check(std::count(xy.commands.begin(),xy.commands.end(),"CURVE?")==1 && axes[0].samples!=axes[1].samples,"XY pair must use one native record");
        PairIO source; source.wrongSource=true; Reject([&]{CaptureChannel(source,1);});
        Check(source.commands.back()=="DATA SOURCE:ACQ,TARGET:REF4,CHANNEL:CH1,ENCDG:ASCII","Wrong source not restored");
        std::cout<<"PASS SAVE before/between/after, timing mismatch, source mismatch, one-transfer XY pair\n";
        ControlsIO io; io.reply="ACQUISITION CURRENT:AVERAGE;";
        Check(ReadControl(io,"CURR")=="AVE","Verbose acquisition mode parsing");
        Check(WriteControl(io,"CURR","AVE")=="AVE","Mode readback");
        Reject([&]{WriteControl(io,"CURR","PEA");});
        auto count=io.commands.size(); Reject([&]{WriteControl(io,"TRIGC","17");});
        Reject([&]{WriteControl(io,"SMO","ON;INIT");});
        Reject([&]{WriteControl(io,"SAV","ON");});
        Reject([&]{WriteControl(io,"LONG","OFF");});
        Check(io.commands.size()==count,"Invalid controls touched instrument");
        io.reply="ACQUISITION TRIGCOUNT:1024;"; Check(WriteControl(io,"TRIGC","1024")=="1024","Trigger count");
        io.reply="CURSOR CHANNEL:CH2,TARGET:ACQUISITION,SELECT:CURS1,POSITION:512;";
        Check(ReadControl(io,"CHA")=="CH2" && ReadControl(io,"TAR")=="ACQ" && ReadControl(io,"POS")=="512","Verbose cursor reply");
        auto setup=ParseSetup("ACQUISITION SMOOTHING:ON,WEIGHT:16,TRIGCOUNT:512,CURRENT:AVERAGE;CURSOR TARGET:ACQUISITION,CHANNEL:CH1,SELECT:CURS1,POSITION:100;PLOT AUTO:OFF,FORMAT:HPGL,GRATICULE:ON,SPEED:3;RQS ON;OPC OFF;LONG ON;REFDISPLAY REF4:OFF;REFFROM ACQUISITION;");
        Check(ParseSetup(BuildSetup(setup))==setup,"SET roundtrip");
        auto built=BuildSetup(setup);
        Check(built.find("CURS TAR")<built.find("CURS CHA") && built.rfind("LONG ON")==built.size()-7,"Setup command ordering");
        for(auto invalid:{"INIT NOW;","ACQ UNKNOWN:ON;","PLOT AUT:ON;","LONG OFF;","ACQ WEI:3;","ACQ SMO:ON,SMO:OFF;","MES 1:\"INJECT\";"}) Reject([&]{ParseSetup(invalid);});
        ControlState state; io.reply="ACQ SMO:ON;LONG ON;";
        state.Execute(io,"setupsave","1"); Check(!state.slots[0].empty(),"Setup save");
        state.Execute(io,"setupload","1"); Reject([&]{state.Execute(io,"setupload","2");});
        count=io.commands.size(); Reject([&]{state.Execute(io,"message","16:x\";INIT");}); Check(io.commands.size()==count,"Message injection reached instrument");
        state.Execute(io,"message","16:Hello Tek"); Check(io.command=="MES 16:\"Hello Tek\"","Message encoding");
        state.Execute(io,"plotstart",""); Check(state.plotHold,"Plot hold"); count=io.commands.size();
        Reject([&]{state.Execute(io,"reset","");}); Check(io.commands.size()==count,"Plot hold allowed traffic");
        state.Execute(io,"plotabort",""); Check(!state.plotHold && io.command=="PLOT AUT:OFF","Plot abort");
        io.reply="ATR AUTO;"; Reject([&]{state.Execute(io,"arm","");});
        io.reply="ATR SGLSWP;"; state.Execute(io,"arm","");
        std::cout<<"PASS control validation/readback, cursor replies, strict setup slots/order, message injection guard, plot hold/abort, single-sweep guard\n";
        Check(argc==2,"Pass fixture directory");
        std::ifstream setupFile(std::string(argv[1])+"/setup-v15.txt"); Check(bool(setupFile),"Cannot read actual V15 setup fixture");
        std::string v15{std::istreambuf_iterator<char>(setupFile),std::istreambuf_iterator<char>()};
        auto decoded=ParseSetup(v15);
        Check(decoded.size()==20 && decoded.at("READOUT")=="ON" && decoded.at("ACQ NUM")=="0","Actual V15 reply must preserve READOUT and NUMSWEEPS");
        Check(ParseSetup(BuildSetup(decoded))==decoded,"Actual V15 setup roundtrip");
        SetupIO setupIo(v15); ControlState setupState;
        setupState.Execute(setupIo,"setupsave","1");
        Check(!setupState.slots[0].empty(),"V15 setup was not saved");
        setupIo.settings["ACQ SMO"]="OFF"; setupIo.settings["ACQ NUM"]="10"; setupIo.settings["READOUT"]="OFF";
        setupState.Execute(setupIo,"setupload","1");
        Check(setupIo.settings==decoded,"Restore did not recover smoothing, sweep limit and readout");
        Check(std::find(setupIo.writes.begin(),setupIo.writes.end(),"ACQ NUM:0")!=setupIo.writes.end(),"Continuous sweep limit not restored");
        setupIo.settings["ACQ SMO"]="OFF"; setupIo.rejectSmooth=true;
        Reject([&]{setupState.Execute(setupIo,"setupload","1");});
        ControlsIO readout; readout.reply=v15; Check(ReadControl(readout,"READOUT")=="ON","READOUT must read SET? without assuming READOUT? support");
        Check(readout.command=="SET?","Undocumented READOUT query was issued");
        ControlsIO num; num.reply="ACQUISITION NUMSWEEPS:0;";
        Check(WriteControl(num,"NUM","0")=="0","NUMSWEEPS zero/continuous parsing");
        auto before=num.commands.size(); Reject([&]{WriteControl(num,"NUM","2048");}); Check(num.commands.size()==before,"Out-of-range sweep limit touched hardware");
        std::cout<<"PASS actual V15 SET? screenshot regression, READOUT, NUMSWEEPS 0, short per-setting restore, smoothing restored, rejected-write detection\n";
        return 0;
    }
    catch(const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<"\n"; return 1; }
}
