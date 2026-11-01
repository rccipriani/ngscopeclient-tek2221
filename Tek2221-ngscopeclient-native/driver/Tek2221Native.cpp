#include "Tek2221Native.h"
#include <regex>
#include <sstream>
#include <locale>
#include <cmath>
#include <stdexcept>
#include <exception>
#include <limits>

namespace Tek2221
{
static std::string Trim(const std::string& text)
{
    auto first=text.find_first_not_of(" \r\n\t");
    if(first==std::string::npos) return {};
    return text.substr(first,text.find_last_not_of(" \r\n\t")-first+1);
}
static double Number(const std::string& text)
{
    std::istringstream s(text); s.imbue(std::locale::classic());
    double x; std::string extra;
    if(!(s>>x) || (s>>extra) || !std::isfinite(x)) throw std::runtime_error("Invalid numeric preamble field: "+text);
    return x;
}
static std::string Field(const std::string& text,const std::string& key)
{
    // Keys below are fixed literals with escaped regex punctuation.
    std::smatch m;
    if(!std::regex_search(text,m,std::regex("(?:^|,)\\s*"+key+"\\s*:\\s*([^,;]+)")))
        throw std::runtime_error("Missing preamble field: "+key);
    return Trim(m[1]);
}
static int64_t Femtoseconds(double value)
{
    long double v=static_cast<long double>(value)*1e15L;
    if(!std::isfinite(v) || v<=std::numeric_limits<int64_t>::min() || v>=std::numeric_limits<int64_t>::max())
        throw std::runtime_error("Time scaling overflow");
    return static_cast<int64_t>(std::round(v));
}
std::string Query(MessageIO& io,const std::string& command)
{
    io.Write(command); auto b=io.ReadMessage();
    for(auto c:b) if((c<32 && c!=9 && c!=10 && c!=13) || c>126) throw std::runtime_error("Non-ASCII reply to "+command);
    return Trim(std::string(b.begin(),b.end()));
}
std::string Identify(MessageIO& io)
{
    auto id=Query(io,"ID?");
    if(!std::regex_match(id,std::regex("ID\\s+TEK/2221(?:A)?,[^\\r\\n]+;?",std::regex::icase)))
        throw std::runtime_error("Expected Tektronix 2221 identity, received: "+id);
    return id;
}
Record Decode(const std::string& text,const std::vector<uint8_t>& raw)
{
    size_t marker=0; while(marker<raw.size() && raw[marker]!='%') ++marker;
    if(marker>32 || marker+3>raw.size()) throw std::runtime_error("Missing legacy CURVE % header");
    auto prefix=Trim(std::string(raw.begin(),raw.begin()+marker));
    if(prefix!="" && prefix!="CURVE" && prefix!="CURV") throw std::runtime_error("Unexpected CURVE prefix");
    size_t n=(size_t(raw[marker+1])<<8)|raw[marker+2];
    if(!n || marker+3+n>raw.size()) throw std::runtime_error("Truncated CURVE block");
    unsigned sum=0; for(size_t i=marker+1;i<marker+3+n;i++) sum+=raw[i];
    if(sum&255) throw std::runtime_error("CURVE checksum mismatch");
    for(size_t i=marker+3+n;i<raw.size();i++)
        if(raw[i]!=';' && raw[i]!='\n' && raw[i]!='\r' && raw[i]!=' ') throw std::runtime_error("Unexpected CURVE trailer");
    auto format=Field(text,"PT\\.FMT");
    if(format!="Y" && format!="ENV" && format!="XY") throw std::runtime_error("Unsupported PT.FMT");
    size_t stride=format=="Y"?1:2;
    if(Field(text,"BN\\.FMT")!="RP" || Field(text,"ENCDG")!="BINARY" || Field(text,"XUNIT")!="S" || Field(text,"YUNIT")!="V")
        throw std::runtime_error("Requires calibrated unsigned binary data in seconds and volts");
    double width=Number(Field(text,"BYT/NR")), bits=Number(Field(text,"BIT/NR")), count=Number(Field(text,"NR\\.PT"));
    if(!((width==1 && bits==8)||(width==2 && bits==16)) || count<1 || count>4096 || std::floor(count)!=count || count*width*stride!=n-1)
        throw std::runtime_error("Point count/sample width mismatch");
    std::smatch id;
    if(!std::regex_search(text,id,std::regex("WFID:\"([^\"]+)\""))) throw std::runtime_error("Missing WFID");
    std::vector<std::string> fields; std::istringstream parts(id[1]); std::string part;
    while(std::getline(parts,part,',')) fields.push_back(Trim(part));
    bool xy=format=="XY";
    if(fields.size()<(xy?7u:5u) || (fields[0]!="ACQ" && fields[0]!="REF4") ||
        (xy ? fields[1]!="XY" : (fields[1]!="CH1" && fields[1]!="CH2")))
        throw std::runtime_error("Expected ACQ/REF4 CH1/CH2 or XY record");
    std::smatch scale;
    if(!std::regex_match(fields[xy?4:2],scale,std::regex("([0-9.+Ee-]+)([mun]?)V"))) throw std::runtime_error("Invalid WFID volts/div");
    double vscale=Number(scale[1]);
    if(scale[2]=="m") vscale*=1e-3; else if(scale[2]=="u") vscale*=1e-6; else if(scale[2]=="n") vscale*=1e-9;
    double dt=Number(Field(text,"XINCR")), mult=Number(Field(text,"YMULT"));
    double offset=Number(Field(text,"YOFF")), point=Number(Field(text,"PT\\.OFF"));
    if(dt<=0 || mult<=0 || vscale<=0 || offset==-10000 || point==-10000)
        throw std::runtime_error("Unknown/invalid scaling or trigger reference");
    Record r; r.raw=raw; r.preamble=text; r.bits=int(bits); r.coupling=fields[xy?5:3]; r.channel=fields[1]; r.source=fields[0]; r.format=format;
    r.interval=Femtoseconds(dt); r.phase=Femtoseconds(-point*dt); r.range=8*vscale;
    if(r.interval<=0 || !std::isfinite(r.range) || (r.coupling!="AC" && r.coupling!="DC" && r.coupling!="GND"))
        throw std::runtime_error("Invalid interval/range/coupling");
    r.samples.resize(size_t(count));
    if(format=="ENV") r.minimum.resize(size_t(count));
    if(xy)
    {
        std::smatch xscale;
        if(!std::regex_match(fields[2],xscale,std::regex("([0-9.+Ee-]+)([mun]?)V")))
            throw std::runtime_error("Invalid XY X scale");
        r.xRange=8*Number(xscale[1]);
        if(xscale[2]=="m") r.xRange*=1e-3; else if(xscale[2]=="u") r.xRange*=1e-6; else if(xscale[2]=="n") r.xRange*=1e-9;
        r.xCoupling=fields[3]; r.xSamples.resize(size_t(count));
        if(r.xRange<=0 || !std::isfinite(r.xRange) || (r.xCoupling!="AC" && r.xCoupling!="DC" && r.xCoupling!="GND"))
            throw std::runtime_error("Invalid XY X range/coupling");
    }
    double xmult=xy?Number(Field(text,"XMULT")):0, xoff=xy?Number(Field(text,"XOFF")):0;
    if(xy && (xmult<=0 || xoff==-10000)) throw std::runtime_error("Invalid XY X calibration");
    auto sample=[&](size_t at,double gain,double off) {
        int code=width==1 ? raw[at] : (int(raw[at])<<8)|raw[at+1];
        float value=float((code-(width==1?128:32768)-off)*gain);
        if(!std::isfinite(value)) throw std::runtime_error("Sample voltage overflow");
        return value;
    };
    for(size_t i=0;i<r.samples.size();i++)
    {
        size_t at=marker+3+i*size_t(width)*stride;
        r.samples[i]=sample(at+(xy?size_t(width):0),mult,offset);
        if(xy) r.xSamples[i]=sample(at,xmult,xoff);
        if(format=="ENV")
        {
            r.minimum[i]=sample(at+size_t(width),mult,offset);
            if(r.minimum[i]>r.samples[i]) throw std::runtime_error("Envelope minimum exceeds maximum");
        }
    }
    return r;
}
Record CaptureSource(MessageIO& io,unsigned channel,const std::string& source,std::function<void(const std::string&)> log)
{
    if(channel!=1 && channel!=2) throw std::runtime_error("Capture channel must be 1 or 2");
    if(source!="ACQ" && source!="REF4") throw std::runtime_error("Source must be ACQ or REF4");
    const auto name="CH"+std::to_string(channel);
    auto say=[&](const std::string& text) { if(log) log(text); };
    say("Native 64-bit capture: ID?");
    auto identity=Identify(io);
    auto original=Query(io,"DATA?");
    // Validate exactly the DATA fields which we will restore. No arbitrary replay.
    if(!std::regex_match(original,std::regex("DATA\\s+SOURCE:(ACQ|REF[1-4]),TARGET:REF[1-4],CHANNEL:(CH1|CH2),ENCDG:(ASCII|BINARY|HEX);?",std::regex::icase)))
        throw std::runtime_error("Cannot safely retain original DATA settings: "+original);
    if(!original.empty() && original.back()==';') original.pop_back();
    Record record; std::exception_ptr failure;
    try
    {
        say("Select "+source+" "+name+" binary");
        io.Write("DATA SOURCE:"+source+",CHANNEL:"+name+",ENCDG:BINARY");
        auto preamble=Query(io,"WFMPRE?");
        io.Write("CURVE?"); auto raw=io.ReadMessage();
        record=Decode(preamble,raw); record.identity=identity;
        if(record.source!=source) throw std::runtime_error("Returned waveform source differs from request");
        if(record.format!="XY" && record.channel!=name) throw std::runtime_error("Requested "+name+" but scope returned "+record.channel+"; check BOTH + ALT on the scope");
    }
    catch(...) { failure=std::current_exception(); }
    // Restore even when selecting, reading, or decoding the waveform failed.
    try { io.Write(original); say("Original DATA settings restore sent"); }
    catch(const std::exception& e)
    {
        throw std::runtime_error(std::string("DATA settings restore failed: ")+e.what()+(failure?" (capture also failed)":""));
    }
    if(failure) std::rethrow_exception(failure);
    say("Native "+name+" captured: "+std::to_string(record.samples.size())+" points, "+std::to_string(record.bits)+" bits, "+std::to_string(record.interval)+" fs/sample");
    return record;
}
Record CaptureChannel(MessageIO& io,unsigned channel,std::function<void(const std::string&)> log)
{ return CaptureSource(io,channel,"ACQ",log); }
bool Saved(MessageIO& io)
{
    auto reply=Query(io,"ACQ? SAVE"); std::smatch m;
    if(!std::regex_match(reply,m,std::regex("ACQ(?:UISITION)?\\s+SAV(?:E)?\\s*:\\s*(ON|OFF);?",std::regex::icase)))
        throw std::runtime_error("Unexpected SAVE reply: "+reply);
    return m[1]=="ON" || m[1]=="on";
}
void ValidatePair(const Record& a,const Record& b)
{
    if(a.source!="ACQ" || b.source!="ACQ" || a.channel!="CH1" || b.channel!="CH2" ||
        a.samples.size()!=b.samples.size() || a.interval!=b.interval || a.phase!=b.phase || a.format!=b.format)
        throw std::runtime_error("Paired records have incompatible channels, formats or timing; pair discarded");
}
Record XYComponent(const Record& r,unsigned channel)
{
    if(r.format!="XY" || (channel!=1 && channel!=2)) throw std::runtime_error("Expected XY record and channel 1/2");
    Record out=r; out.channel=channel==1?"CH1":"CH2";
    if(channel==1) { out.samples=r.xSamples; out.range=r.xRange; out.coupling=r.xCoupling; }
    out.xSamples.clear(); return out;
}
std::vector<Record> CapturePair(MessageIO& io,std::function<void(const std::string&)> log)
{
    auto held=[&]() { if(!Saved(io)) throw std::runtime_error("Paired capture requires BOTH + ALT and SAVE; pair discarded"); };
    held(); auto a=CaptureChannel(io,1,log); held();
    Record b;
    if(a.format=="XY") { b=XYComponent(a,2); a=XYComponent(a,1); }
    else b=CaptureChannel(io,2,log);
    held(); ValidatePair(a,b); return {std::move(a),std::move(b)};
}
Record CaptureCH1(MessageIO& io,std::function<void(const std::string&)> log)
{ return CaptureChannel(io,1,log); }
}
