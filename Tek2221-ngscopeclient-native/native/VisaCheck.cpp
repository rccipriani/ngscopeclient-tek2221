#include "Tek2221VISA.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <stdexcept>
static void Save(const Tek2221::Record& r,const std::filesystem::path& folder)
{
    namespace fs=std::filesystem; fs::create_directories(folder);
    std::ofstream pre(folder/"preamble.txt"),raw(folder/"curve-response.bin",std::ios::binary),samples(folder/"samples.csv");
    pre.exceptions(std::ios::failbit|std::ios::badbit); raw.exceptions(std::ios::failbit|std::ios::badbit); samples.exceptions(std::ios::failbit|std::ios::badbit);
    pre<<r.preamble<<"\n"; raw.write(reinterpret_cast<const char*>(r.raw.data()),r.raw.size());
    samples<<"time_s,bnc_volts";
    if(!r.minimum.empty()) samples<<",minimum_bnc_volts";
    if(!r.xSamples.empty()) samples<<",x_bnc_volts";
    samples<<"\n"<<std::setprecision(17);
    for(size_t i=0;i<r.samples.size();++i)
    {
        samples<<(double(r.phase)+double(i)*r.interval)/1e15<<","<<r.samples[i];
        if(!r.minimum.empty()) samples<<","<<r.minimum[i];
        if(!r.xSamples.empty()) samples<<","<<r.xSamples[i];
        samples<<"\n";
    }
    std::cout<<"Saved "<<r.source<<" "<<r.channel<<" in "<<fs::absolute(folder).string()<<"\n";
}
int main(int argc,char** argv)
{
    try
    {
        std::cout<<"Tek2221 native VISA check v0.4; process bits="<<8*sizeof(void*)<<"\n";
        if(argc==2 && std::string(argv[1])=="--load-only")
        {
            Tek2221::VisaSession visa;
            std::cout<<"Loaded: "<<visa.LibraryPath()<<"\nPASS: 64-bit VISA resource manager opened; no instrument access.\n"; return 0;
        }
        if(argc<3 || argc>4) throw std::runtime_error("Usage: --load-only | --id RESOURCE | --capture[-ch2] RESOURCE [folder] | --ref4[-ch2] RESOURCE [folder] | --pair RESOURCE [folder]");
        std::string mode=argv[1];
        if(mode!="--id" && mode!="--capture" && mode!="--capture-ch2" && mode!="--ref4" && mode!="--ref4-ch2" && mode!="--pair") throw std::runtime_error("Unknown diagnostic mode");
        Tek2221::VisaSession visa(argv[2]); std::cout<<"Loaded: "<<visa.LibraryPath()<<"\n";
        if(mode=="--id") { std::cout<<"Identity: "<<Tek2221::Identify(visa)<<"\nPASS: native 64-bit GPIB identification.\n"; return 0; }
        auto log=[](const std::string& msg){std::cout<<msg<<"\n";};
        std::vector<Tek2221::Record> records;
        if(mode=="--pair") records=Tek2221::CapturePair(visa,log);
        else records.push_back(Tek2221::CaptureSource(visa,mode.find("ch2")!=std::string::npos?2:1,mode.find("ref4")!=std::string::npos?"REF4":"ACQ",log));
        for(const auto& r:records)
        {
            auto mm=std::minmax_element(r.samples.begin(),r.samples.end());
            std::cout<<r.preamble<<"\nPASS: "<<r.source<<" "<<r.channel<<" "<<r.format<<" "<<r.samples.size()<<" points, "<<r.bits<<" bits; "<<r.interval<<" fs/sample; phase "<<r.phase<<" fs; "
                <<std::setprecision(10)<<(*mm.second-*mm.first)<<" V p-p at BNC (before probe factor).\n";
            if(argc==4) Save(r,mode=="--pair"?std::filesystem::path(argv[3])/r.channel:std::filesystem::path(argv[3]));
        }
        if(mode=="--pair") std::cout<<"PAIR PASS: SAVE checked before/between/after; count, interval, time zero and format match. ALT sampling is sequential, not simultaneous.\n";
        return 0;
    }
    catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<"\n"; return 1; }
}
