#include "performance_metrics.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

namespace {
int failures=0;
void check(bool value,const char* expression,int line){if(!value){std::cerr<<"FAIL "<<line<<": "<<expression<<'\n';++failures;}}
#define CHECK(x) check((x),#x,__LINE__)
}

int main(){
    using namespace threadlight::performance;
    FrameHistogram histogram;
    for(double value : {5.0,10.0,15.01,20.0,1500.0})histogram.add(value);
    histogram.add(0);histogram.add(-1);
    CHECK(histogram.count()==5);
    CHECK(histogram.percentile(.5)==15.1);
    CHECK(histogram.percentile(.95)==1500.0);
    CHECK(histogram.maxMs()==1500.0);
    CHECK(histogram.percentile(std::numeric_limits<double>::quiet_NaN())==0.0);
    CHECK(histogram.percentile(std::numeric_limits<double>::infinity())==0.0);

    const auto path=std::filesystem::current_path()/
        ("ThreadLightPerfTest-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".csv");
    CHECK(!std::filesystem::exists(path));
    Recorder recorder;
    CHECK(recorder.open(path));
    Frame sample;sample.number=1;sample.intervalMs=16.667;sample.discardedMs=4.5;
    sample.mode="play";sample.phase="spawning";sample.wave=2;sample.enemies=4;
    sample.memory={123,456,true};
    CHECK(recorder.record(sample));
    recorder.finish();CHECK(recorder.healthy());
    CHECK(recorder.intervals().count()==1);
    CHECK(recorder.discardedFrames()==1 && recorder.discardedMs()==4.5);
    Recorder second;CHECK(!second.open(path)); // Existing files are never truncated.
    std::ifstream input(path);std::string header,row;
    std::getline(input,header);std::getline(input,row);input.close();
    CHECK(std::count(header.begin(),header.end(),',')==24);
    CHECK(std::count(row.begin(),row.end(),',')==24);
    CHECK(row.find(",play,spawning,2,")!=std::string::npos);
    CHECK(row.ends_with(",123,456"));
    std::error_code error;std::filesystem::remove(path,error);CHECK(!error);
    if(failures)return EXIT_FAILURE;
    std::cout<<"Performance histogram and CSV checks passed.\n";
}
