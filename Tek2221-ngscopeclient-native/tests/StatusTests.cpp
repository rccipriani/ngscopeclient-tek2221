#include "Tek2221Status.h"
#include <stdexcept>
#include <iostream>
using namespace Tek2221;
static void Check(bool v,const char* message) { if(!v) throw std::runtime_error(message); }
int main()
{
    try
    {
        PanelStatus s;
        s.Begin("Save scope settings to selected session slot");
        s.Complete(false,"Save scope settings to selected session slot","Saved scope settings to session slot 1",true);
        auto saved=s.action.text;
        for(int i=0;i<4;++i)
        {
            s.Capture("Capture OK",true);
            s.Begin("Status / REF4");
            Check(s.action.text==saved,"Starting a query overwrote Save confirmation");
            s.Complete(true,"Status / REF4","REFDISP REF4:ON;",true);
            Check(s.action.text==saved,"Capture/query overwrote Save confirmation");
        }
        s.Capture("Capture error: timeout",false);
        s.Complete(true,"Status / REF4","Error: timeout",false);
        Check(s.action.text==saved && s.capture.kind==StatusKind::Error && s.query.kind==StatusKind::Error,"Error categories leaked");
        s.Begin("Restore selected session setup slot");
        Check(s.action.text==saved,"Pending action erased previous completed result");
        s.Complete(false,"Restore selected session setup slot","Error: setup readback differs for ACQ SMO",false);
        Check(s.action.kind==StatusKind::Error && s.action.text.find("Restore selected")!=std::string::npos,"Action error missing operation name");
        s.Capture("Paired capture OK",true);
        Check(s.action.kind==StatusKind::Error && s.operation.empty(),"Capture cleared action error");
        s.Complete(false,"Acquisition / Smoothing = ON","ON",true);
        Check(s.action.kind==StatusKind::Success && s.action.text.find("Smoothing")!=std::string::npos,"Setting write not reported as an action");
        std::cout<<"PASS Save confirmation survives captures, queries/errors and pending requests; action results identify operation and replace only on completed action/write\n";
        return 0;
    }
    catch(const std::exception& e) { std::cerr<<"FAIL "<<e.what()<<"\n"; return 1; }
}
