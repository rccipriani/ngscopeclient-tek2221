#include "Tek2221Controls.h"
#include <algorithm>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <cctype>
namespace Tek2221 {
const std::vector<ControlSpec>& Controls()
{
    static const std::vector<ControlSpec> fields={
        {"SMO","Acquisition / Smoothing","ACQ? SMO","ACQ SMO:","SMO",{"ON","OFF"}},
        {"VEC","Acquisition / Vectors","ACQ? VEC","ACQ VEC:","VEC",{"ON","OFF"}},
        {"WEI","Acquisition / Average weight","ACQ? WEI","ACQ WEI:","WEI",{"1","2","4","8","16","32","64","128","256"}},
        {"NUM","Acquisition / Sweep limit (0 = continuous)","ACQ? NUM","ACQ NUM:","NUM",{},0,2047,1},
        {"TRIGC","Acquisition / Trigger count","ACQ? TRIGC","ACQ TRIGC:","TRIGC",{},16,4080,4},
        {"CURR","Acquisition / Current sampling mode","ACQ? CURR","ACQ CURR:","CURR",{"SAM","PEA","AVE","ACC","DEF"}},
        {"HSR","Acquisition / High sample rate","ACQ? HSR","ACQ HSR:","HSR",{"SAM","AVE","ACC"}},
        {"LSR","Acquisition / Low sample rate","ACQ? LSR","ACQ LSR:","LSR",{"SAM","PEA","AVE","ACC"}},
        {"REP","Acquisition / Repetitive","ACQ? REP","ACQ REP:","REP",{"SAM","AVE","ACC"}},
        {"ROL","Acquisition / Roll","ACQ? ROL","ACQ ROL:","ROL",{"SAM","PEA"}},
        {"SCA","Acquisition / Scan","ACQ? SCA","ACQ SCA:","SCA",{"SAM","PEA","AVE","ACC"}},
        {"SAV","Status / SAVE (front panel)","ACQ? SAV","","",{}},
        {"STOR","Status / Storage (front panel)","STOR?","","",{}},
        {"CHA","Cursors / Channel","CURS? CHA","CURS CHA:","CHA",{"CH1","CH2"}},
        {"TAR","Cursors / Target","CURS? TAR","CURS TAR:","TAR",{"ACQ","REF4"}},
        {"SEL","Cursors / Selected cursor","CURS? SEL","CURS SEL:","SEL",{"CURS1","CURS2"}},
        {"POS","Cursors / Position","CURS? POS","CURS POS:","POS",{},0,4095,1},
        {"FOR","Legacy plot / Format","PLOT?","PLOT FOR:","FOR",{"XY","HPG","EPS7","EPS8","TJE"}},
        {"GRA","Legacy plot / Graticule","PLOT?","PLOT GRA:","GRA",{"ON","OFF"}},
        {"SPE","Legacy plot / Pen speed","PLOT?","PLOT SPE:","SPE",{},1,10,1},
        {"AUT","Legacy plot / Automatic plotting","PLOT?","PLOT AUT:","AUT",{"OFF","ON"}},
        {"RQS","Interface / Service requests","RQS?","RQS ","",{"ON","OFF"}},
        {"OPC","Interface / Operation complete events","OPC?","OPC ","",{"ON","OFF"}},
        {"READOUT","Interface / CRT readout","SET?","READOUT ","",{"ON","OFF"}},
        {"LONG","Interface / Verbose replies","LONG?","LONG ","",{"ON","OFF"}},
        {"REFD","Status / REF4","REFD?","","",{}},
        {"REFF","Status / Reference source","REFF?","","",{}},
        {"ATR","Status / Trigger mode","ATR?","","",{}},
        {"TRI","Status / Trigger indicator","TRI?","","",{}},
        {"SGL","Status / Single sweep","SGL?","","",{}},
        {"HOR","Status / Horizontal settings","HOR?","","",{}},
        {"VMO","Status / Vertical mode","VMO?","","",{}},
        {"CH1","Status / CH1 settings","CH1?","","",{}},
        {"CH2","Status / CH2 settings","CH2?","","",{}},
        {"PROB1","Status / CH1 reported probe","PROB? CH1","","",{}},
        {"PROB2","Status / CH2 reported probe","PROB? CH2","","",{}},
        {"DIS","Status / Roll or scan","ACQ? DIS","","",{}},
        {"POI","Status / Acquisition points","ACQ? POI","","",{}},
        {"SWP","Status / Acquisition sweeps","ACQ? SWP","","",{}},
        {"TRIGM","Status / Trigger position mode","ACQ? TRIGM","","",{}},
        {"DV","Status / Scope cursor delta V","DELTAV?","","",{}},
        {"DT","Status / Scope cursor delta T","DELTAT?","","",{}},
        {"ID","Status / Identity","ID?","","",{}},
        {"HELP","Status / Command headers","HELP?","","",{}},
        {"SET","Status / Writable setup","SET?","","",{}}
    }; return fields;
}
const std::vector<std::pair<std::string,std::string>>& Actions()
{
    static const std::vector<std::pair<std::string,std::string>> actions={
        {"reset","Reset acquisition modes"},{"initialize","Initialize acquisition defaults"},
        {"arm","Arm hardware single sweep (select SGL SWP first)"},
        {"refsave","Save acquisition to REF4 (overwrite)"},{"refshow","Show REF4"},
        {"refhide","Hide REF4"},{"referase","Erase REF4"},
        {"setupsave","Save scope settings to selected session slot"},
        {"setupload","Restore selected session setup slot"},
        {"message","Show CRT message"},{"clear","Clear CRT message"},
        {"event","Read next event (consumes event)"},
        {"plotstart","Start legacy plot (pauses transfers)"},
        {"plotabort","Abort plot / disable automatic plotting"}
    }; return actions;
}
static std::string Upper(std::string v)
{ for(auto& c:v) c=char(std::toupper(static_cast<unsigned char>(c))); return v; }
static std::string Trim(std::string v)
{
    auto a=v.find_first_not_of(" \t\r\n"); if(a==std::string::npos) return {};
    return v.substr(a,v.find_last_not_of(" \t\r\n")-a+1);
}
static const ControlSpec& Spec(const std::string& key)
{
    for(const auto& f:Controls()) if(f.key==key) return f;
    throw std::runtime_error("Unknown control: "+key);
}
static std::string Validate(const ControlSpec& f,const std::string& value)
{
    if(f.command.empty()) throw std::runtime_error("Read-only scope status");
    if(!f.choices.empty())
    {
        for(auto c:f.choices) if(c==value) return value;
    }
    else if(std::regex_match(value,std::regex("[0-9]{1,4}")))
    {
        int n=std::stoi(value);
        if(n>=f.minimum && n<=f.maximum && n%f.step==0) return std::to_string(n);
    }
    throw std::runtime_error("Invalid value for "+f.label);
}
static std::string Extract(const ControlSpec& f,const std::string& reply)
{
    std::smatch m;
    auto pattern=f.field.empty()?"^[A-Z]+\\s+([A-Z0-9]+)\\s*;?$":"(?:^|[ ,])"+f.field+"[A-Z]*\\s*:\\s*([A-Z0-9]+)";
    auto u=Upper(reply);
    if(!std::regex_search(u,m,std::regex(pattern))) throw std::runtime_error("Unexpected control reply: "+reply);
    auto v=m[1].str();
    // Normalize verbose mode names to their documented unambiguous command abbreviations.
    for(auto c:f.choices) if(v==c || (c.size()>=3 && v.compare(0,c.size(),c)==0)) return c;
    return Validate(f,v);
}
std::string ReadControl(MessageIO& io,const std::string& key)
{
    const auto& f=Spec(key);
    auto reply=Query(io,f.query);
    if(key=="READOUT")
    {
        std::smatch m;
        if(!std::regex_search(reply,m,std::regex("(?:^|;)\\s*READOUT\\s+(ON|OFF)\\s*(?:;|$)",std::regex::icase)))
            throw std::runtime_error("Missing READOUT in SET? reply: "+reply);
        return Upper(m[1]);
    }
    return f.command.empty()?reply:Extract(f,reply);
}
std::string WriteControl(MessageIO& io,const std::string& key,const std::string& value)
{
    const auto& f=Spec(key); auto wanted=Validate(f,value);
    if(key=="LONG" && wanted=="OFF") throw std::runtime_error("Native capture requires LONG ON");
    io.Write(f.command+wanted);
    if(key=="AUT" && wanted=="ON") return "ON"; // subsequent queries are forbidden while plotting
    auto actual=ReadControl(io,key);
    if(wanted!="DEF" && actual!=wanted) throw std::runtime_error("Scope readback differs: requested "+wanted+", got "+actual);
    return actual;
}
std::map<std::string,std::string> ParseSetup(const std::string& text)
{
    if(text.size()>65536 || !std::regex_match(text,std::regex("[A-Za-z0-9_:,;. \\t\\r\\n+\\-]+")))
        throw std::runtime_error("Invalid setup text");
    std::map<std::string,std::string> values;
    std::istringstream lines(Upper(text)); std::string line;
    while(std::getline(lines,line,';'))
    {
        line=Trim(line); if(line.empty()) continue;
        auto space=line.find_first_of(" \t");
        if(space==std::string::npos) throw std::runtime_error("Invalid setup statement: "+line);
        auto root=line.substr(0,space), body=Trim(line.substr(space));
        if(root.compare(0,3,"ACQ")==0) root="ACQ";
        if(root.compare(0,4,"CURS")==0) root="CURS";
        if(root.compare(0,3,"PLO")==0) root="PLOT";
        if(root.compare(0,3,"LON")==0) root="LONG";
        if(root=="OPC" || root=="RQS" || root=="LONG" || root=="READOUT")
        { if(root=="LONG" && body!="ON") throw std::runtime_error("Native capture requires LONG ON");
          if(!values.emplace(root,Validate(Spec(root),body)).second) throw std::runtime_error("Duplicate setup field");
          continue; }
        if(root.compare(0,4,"REFD")==0) root="REFD";
        if(root.compare(0,4,"REFF")==0) root="REFF";
        if(root=="REFF" && body.compare(0,3,"ACQ")==0) { values.emplace("REFF","ACQ"); continue; }
        std::istringstream args(body); std::string arg;
        while(std::getline(args,arg,','))
        {
            auto colon=arg.find(':'); if(colon==std::string::npos) throw std::runtime_error("Invalid setup argument in "+root+": "+arg);
            auto key=Trim(arg.substr(0,colon)), value=Trim(arg.substr(colon+1));
            if(root=="REFD" && key=="REF4")
            {
                if(value=="EMPTY") continue;
                if(value!="ON" && value!="OFF") throw std::runtime_error("Invalid reference display");
                if(!values.emplace("REFD REF4",value).second) throw std::runtime_error("Duplicate setup field");
                continue;
            }
            const ControlSpec* found=nullptr;
            for(const auto& f:Controls()) if(!f.field.empty() && f.command.compare(0,root.size()+1,root+" ")==0 && key.compare(0,f.field.size(),f.field)==0)
            { found=&f; break; }
            if(!found) throw std::runtime_error("Unknown setup field: "+root+" "+key);
            // Reuse readback normalization and strict allowed-value validation.
            value=Extract(*found,root+" "+key+":"+value);
            if(found->key=="AUT" && value=="ON") throw std::runtime_error("Disable automatic plotting before saving a setup");
            if(!values.emplace(root+" "+found->field,value).second) throw std::runtime_error("Duplicate setup field");
        }
    }
    if(values.empty()) throw std::runtime_error("Empty setup");
    return values;
}
std::string BuildSetup(const std::map<std::string,std::string>& values)
{
    std::string out;
    // Cursor target must precede channel/selection/position. LONG goes last.
    const std::vector<std::string> late={"CURS TAR","CURS CHA","CURS SEL","CURS POS","LONG"};
    auto append=[&](const std::string& key,const std::string& value) { if(!out.empty()) out+=';'; out+=key+(key.find(' ')==std::string::npos?" ":":")+value; };
    for(const auto& p:values) if(std::find(late.begin(),late.end(),p.first)==late.end()) append(p.first,p.second);
    for(const auto& key:late) { auto it=values.find(key); if(it!=values.end()) append(it->first,it->second); }
    // Validate the rebuilt command too, so callers cannot bypass the parser with a hand-built map.
    ParseSetup(out); return out;
}
std::string ControlState::Execute(MessageIO& io,const std::string& action,const std::string& arg)
{
    if(plotHold && action!="plotabort") throw std::runtime_error("Plot active: use Abort plot first");
    std::string command, query;
    if(action=="reset") { command="ACQ RES"; query="ACQ?"; }
    else if(action=="initialize") { command="INIT"; query="ACQ?"; }
    else if(action=="arm")
    {
        if(!std::regex_search(Query(io,"ATR?"),std::regex("\\bSGL(?:SWP)?\\b",std::regex::icase)))
            throw std::runtime_error("Select SGL SWP on the scope before arming");
        command="SGL ARM"; query="SGL?";
    }
    else if(action=="refsave") { command="REFF ACQ;SAV REF4"; query="REFD?"; }
    else if(action=="refshow") { command="REFD REF4:ON"; query="REFD?"; }
    else if(action=="refhide") { command="REFD REF4:OFF"; query="REFD?"; }
    else if(action=="referase") { command="REFD REF4:EMPTY"; query="REFD?"; }
    else if(action=="setupsave" || action=="setupload")
    {
        if(!std::regex_match(arg,std::regex("[1-9]|10"))) throw std::runtime_error("Setup slot must be 1..10");
        auto& slot=slots.at(std::stoul(arg)-1);
        if(action=="setupsave") { auto text=Query(io,"SET?"); ParseSetup(text); slot=text; return "Saved scope settings to session slot "+arg; }
        if(slot.empty()) throw std::runtime_error("Selected setup slot is empty");
        auto wanted=ParseSetup(slot);
        // Use short, individually terminated messages instead of one potentially oversized setup line.
        std::istringstream commands(BuildSetup(wanted)); std::string statement;
        while(std::getline(commands,statement,';')) io.Write(statement);
        auto actual=ParseSetup(Query(io,"SET?"));
        for(const auto& item:wanted) if(actual[item.first]!=item.second) throw std::runtime_error("Setup readback differs for "+item.first);
        return "Restored session slot "+arg+" (analog knobs and waveform memory are not included)";
    }
    else if(action=="message")
    {
        auto colon=arg.find(':'); auto row=arg.substr(0,colon);
        if(colon==std::string::npos || !std::regex_match(row,std::regex("[1-9]|1[0-6]"))) throw std::runtime_error("Message row must be 1..16");
        auto message=arg.substr(colon+1);
        if(message.size()>40 || !std::all_of(message.begin(),message.end(),[](unsigned char c){return c>=32 && c<=126 && c!='"';}))
            throw std::runtime_error("Message must be <=40 printable ASCII characters without quotes");
        command="MES "+row+":\""+message+"\"";
    }
    else if(action=="clear") command="MES 0";
    else if(action=="event") return Query(io,"EVENT?")+" (event consumed)";
    else if(action=="plotstart") { plotHold=true; io.Write("PLOT STA"); return "Plot requested; transfers paused. Use Abort plot to resume."; }
    else if(action=="plotabort") { io.Write("PLOT ABO"); io.Write("PLOT AUT:OFF"); plotHold=false; return "Plot abort sent; automatic plotting disabled. Press Run/Single to resume."; }
    else throw std::runtime_error("Unknown action");
    io.Write(command);
    if(query.empty()) return command+" sent";
    auto reply=Query(io,query);
    if(action=="refsave" && reply.find("EMPTY")!=std::string::npos) throw std::runtime_error("Reference save still reports EMPTY");
    if(action=="referase" && reply.find("EMPTY")==std::string::npos) throw std::runtime_error("Reference erase not confirmed: "+reply);
    if(action=="refshow" && !std::regex_search(reply,std::regex(":\\s*ON\\b",std::regex::icase))) throw std::runtime_error("Reference show not confirmed: "+reply);
    if(action=="refhide" && !std::regex_search(reply,std::regex(":\\s*(OFF|EMPTY)\\b",std::regex::icase))) throw std::runtime_error("Reference hide not confirmed: "+reply);
    return reply;
}
}
