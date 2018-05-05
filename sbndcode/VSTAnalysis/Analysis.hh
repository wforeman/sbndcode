#ifndef Analysis_hh
#define Analysis_hh

#include <vector>
#include <string>
#include <ctime>
#include <chrono>
#include <numeric>

#include "TROOT.h"
#include "TTree.h"

#include "art/Framework/Core/EDAnalyzer.h"

#include "canvas/Utilities/InputTag.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Principal/Run.h" 
#include "art/Framework/Principal/SubRun.h" 
#include "lardataobj/RawData/RawDigit.h"

#include "ChannelData.hh"
#include "HeaderData.hh"
#include "FFT.hh"
#include "Noise.hh"

/*
  * Main analysis code of the online Monitoring.
  * Takes as input raw::RawDigits and produces
  * a number of useful metrics all defined in 
  * ChannelData.hh
*/

namespace daqAnalysis {
  class Analysis;
  class Timing;
}

// keep track of timing information
class daqAnalysis::Timing {
public:
  std::chrono::time_point<std::chrono::high_resolution_clock> start;
  float fill_waveform;
  float baseline_calc;
  float execute_fft;
  float calc_threshold;
  float find_peaks;
  float calc_noise;
  float reduce_data;
  float coherent_noise_calc;
  float copy_headers;

  Timing():
    fill_waveform(0),
    baseline_calc(0),
    execute_fft(0),
    calc_threshold(0),
    find_peaks(0),
    calc_noise(0),
    reduce_data(0),
    coherent_noise_calc(0),
    copy_headers(0)
  {}
  
  void StartTime();
  void EndTime(float *field);

  void Print();
};


class daqAnalysis::Analysis {
public:
  explicit Analysis(fhicl::ParameterSet const & p);

  // actually analyze stuff
  void AnalyzeEvent(art::Event const & e);


  // configuration
  struct AnalysisConfig {
    public:
    std::string output_file_name;
    float frame_to_dt;
    bool verbose;
    int n_events;
    art::InputTag daq_tag;
    int static_input_size;

    int n_headers;
    bool header_index;

    float threshold;
    float threshold_sigma;
    
    unsigned baseline_calc;
    unsigned n_mode_skip;
    unsigned noise_range_sampling;
    bool use_planes;
    unsigned threshold_calc;
    unsigned n_noise_samples;
    unsigned n_smoothing_samples;
    unsigned n_above_threshold;

    bool sum_waveforms;
    bool fft_per_channel;
    bool fill_waveforms;
    bool reduce_data;
    bool timing;

    AnalysisConfig(const fhicl::ParameterSet &param);
    AnalysisConfig() {}
  };

  // other functions
  void ProcessChannel(const raw::RawDigit &digits);
  void ProcessHeader(const daqAnalysis::HeaderData &header);

  // if the containers filled by the analysis are ready to be processed
  bool ReadyToProcess();
  bool EmptyEvent();

  // configuration is available publicly
  AnalysisConfig _config;
  // keeping track of wire id to index into stuff from Decoder
  std::vector<unsigned> _channel_index_map;
  // output containers of analysis code. Only use after calling ReadyToProcess()
  std::vector<daqAnalysis::ChannelData> _per_channel_data;
  std::vector<daqAnalysis::ReducedChannelData> _per_channel_data_reduced;
  std::vector<daqAnalysis::NoiseSample> _noise_samples;
  std::vector<daqAnalysis::HeaderData> _header_data;
  std::vector<RunningThreshold> _thresholds;
  std::vector<std::vector<int16_t>> _fem_summed_waveforms;

private:
  // Declare member data here.
  unsigned _event_ind;
  FFTManager _fft_manager;
  // keep track of timing data (maybe)
  daqAnalysis::Timing _timing;
  // whether we have analyzed stuff
  bool _analyzed;
};



#endif /* Analysis_hh */
