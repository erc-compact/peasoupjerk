#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctime>
#include <iostream>
#include <map>
#include <fstream>
#include <utils/xml_util.hpp>
#include <utils/cmdline.hpp>
#include <utils/stopwatch.hpp>
#include <data_types/header.hpp>
#include <iomanip>   // for std::setprecision
#include <sstream>   // for std::stringstream
#include "cuda.h"
#include <data_types/filterbank.hpp>

class OutputFileWriter {
  XML::Element root;

public:
  OutputFileWriter()
    : root("peasoup_search")
  {}

  std::string to_string() {
    return root.to_string(true);
  }

  std::string format_float_to_precision(float value, int precision) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(precision) << value;
    return ss.str();
  }

  void to_file(const std::string& filename) {
    std::ofstream out(filename.c_str(), std::ios::binary);
    ErrorChecker::check_file_error(out, filename);
    out << root.to_string(true);
    ErrorChecker::check_file_error(out, filename);
  }

  void add_header(const std::string& filename) {
    std::ifstream in(filename.c_str(), std::ios::binary);
    ErrorChecker::check_file_error(in, filename);
    SigprocHeader hdr;
    read_header(in, hdr);

    XML::Element hdr_el("header_parameters");
    hdr_el.append(XML::Element("source_name",      hdr.source_name));
    hdr_el.append(XML::Element("rawdatafile",      hdr.rawdatafile));
    hdr_el.append(XML::Element("az_start",         hdr.az_start));
    hdr_el.append(XML::Element("za_start",         hdr.za_start));
    hdr_el.append(XML::Element("src_raj",          hdr.src_raj));
    hdr_el.append(XML::Element("src_dej",          hdr.src_dej));
    hdr_el.append(XML::Element("tstart",           hdr.tstart));
    hdr_el.append(XML::Element("tsamp",            hdr.tsamp));
    hdr_el.append(XML::Element("period",           hdr.period));
    hdr_el.append(XML::Element("fch1",             hdr.fch1));
    hdr_el.append(XML::Element("foff",             hdr.foff));
    hdr_el.append(XML::Element("nchans",           hdr.nchans));
    hdr_el.append(XML::Element("telescope_id",     hdr.telescope_id));
    hdr_el.append(XML::Element("machine_id",       hdr.machine_id));
    hdr_el.append(XML::Element("data_type",        hdr.data_type));
    hdr_el.append(XML::Element("ibeam",            hdr.ibeam));
    hdr_el.append(XML::Element("nbeams",           hdr.nbeams));
    hdr_el.append(XML::Element("nbits",            hdr.nbits));
    hdr_el.append(XML::Element("barycentric",      hdr.barycentric));
    hdr_el.append(XML::Element("pulsarcentric",    hdr.pulsarcentric));
    hdr_el.append(XML::Element("nbins",            hdr.nbins));
    hdr_el.append(XML::Element("nsamples",         hdr.nsamples));
    hdr_el.append(XML::Element("nifs",             hdr.nifs));
    hdr_el.append(XML::Element("npuls",            hdr.npuls));
    hdr_el.append(XML::Element("refdm",            hdr.refdm));
    hdr_el.append(XML::Element("signed",           (int)hdr.signed_data));
    root.append(hdr_el);
  }

  void add_segment_parameters( SigprocFilterbank& f) {
    XML::Element seg("segment_parameters");
    seg.append(XML::Element("segment_start_sample", f.get_start_sample()));
    seg.append(XML::Element("segment_nsamples",     f.get_effective_nsamps()));
    seg.append(XML::Element("segment_pepoch",       f.get_segment_pepoch()));
    root.append(seg);
  }

  void add_search_parameters(CmdLineOptions& args) {
    XML::Element search_options("search_parameters");
    search_options.append(XML::Element("infilename",       args.infilename));
    search_options.append(XML::Element("outdir",           args.outdir));
    search_options.append(XML::Element("killfilename",     args.killfilename));
    search_options.append(XML::Element("zapfilename",      args.zapfilename));
    search_options.append(XML::Element("max_num_threads",  args.max_num_threads));
    search_options.append(XML::Element("size",             args.size));
    search_options.append(XML::Element("dmfilename",       args.dm_file));
    search_options.append(XML::Element("cdm",              format_float_to_precision(args.cdm,4)));
    search_options.append(XML::Element("dm_start",         args.dm_start));
    search_options.append(XML::Element("dm_end",           args.dm_end));
    search_options.append(XML::Element("dm_tol",           args.dm_tol));
    search_options.append(XML::Element("dm_pulse_width",   args.dm_pulse_width));
    search_options.append(XML::Element("boundary_5_freq",  args.boundary_5_freq));
    search_options.append(XML::Element("boundary_25_freq", args.boundary_25_freq));
    search_options.append(XML::Element("nharmonics",       args.nharmonics));
    search_options.append(XML::Element("npdmp",            args.npdmp));
    search_options.append(XML::Element("min_snr",          args.min_snr));
    search_options.append(XML::Element("min_freq",         args.min_freq));
    search_options.append(XML::Element("max_freq",         args.max_freq));
    search_options.append(XML::Element("max_harm",         args.max_harm));
    search_options.append(XML::Element("freq_tol",         args.freq_tol));
    search_options.append(XML::Element("verbose",          args.verbose));
    search_options.append(XML::Element("progress_bar",     args.progress_bar));
    root.append(search_options);

    // —— embed the accel+jerk list directly from the file ——
    if (!args.acc_jerk_file.empty()) {
      XML::Element aj_trials("acceleration_jerk_trials");
      aj_trials.add_attribute("file", args.acc_jerk_file);
      std::ifstream ajf(args.acc_jerk_file);
      float a, j; size_t id=0;
      while (ajf >> a >> j) {
        XML::Element trial("trial");
        trial.add_attribute("id", id++);
        std::ostringstream tmp;
        tmp << format_float_to_precision(a,4)
            << "," << format_float_to_precision(j,4);
        trial.set_text(tmp.str());
        aj_trials.append(trial);
      }
      root.append(aj_trials);
    }
  }

  void add_misc_info() {
    XML::Element info("misc_info");
    char buf[128];
    getlogin_r(buf, sizeof(buf));
    std::time_t t = std::time(nullptr);
    std::strftime(buf, sizeof(buf), "%Y-%m-%d-%H:%M", std::localtime(&t));
    info.append(XML::Element("local_datetime", buf));
    std::strftime(buf, sizeof(buf), "%Y-%m-%d-%H:%M", std::gmtime(&t));
    info.append(XML::Element("utc_datetime",   buf));
    root.append(info);
  }

  void add_timing_info(const std::map<std::string,Stopwatch>& elapsed_times) {
    XML::Element times("execution_times");
    for (auto& kv : elapsed_times)
      times.append(XML::Element(kv.first, kv.second.getTime()));
    root.append(times);
  }

  void add_gpu_info(const std::vector<int>& device_idxs) {
    XML::Element gpu("cuda_device_parameters");
    int rv, dv; cudaRuntimeGetVersion(&rv); cudaDriverGetVersion(&dv);
    gpu.append(XML::Element("runtime", rv));
    gpu.append(XML::Element("driver",  dv));
    cudaDeviceProp prop;
    for (auto id : device_idxs) {
      XML::Element dev("cuda_device");
      dev.add_attribute("id", id);
      cudaGetDeviceProperties(&prop, id);
      dev.append(XML::Element("name",     prop.name));
      dev.append(XML::Element("major_cc", prop.major));
      dev.append(XML::Element("minor_cc", prop.minor));
      gpu.append(dev);
    }
    root.append(gpu);
  }

  void add_dm_list(const std::vector<float>& dms) {
    XML::Element dm_trials("dedispersion_trials");
    dm_trials.add_attribute("count", dms.size());
    for (size_t i=0; i<dms.size(); ++i) {
      XML::Element trial("trial");
      trial.add_attribute("id", i);
      std::ostringstream ss;
      ss << std::fixed << std::setprecision(4) << dms[i];
      trial.set_text(ss.str());
      dm_trials.append(trial);
    }
    root.append(dm_trials);
  }

  void add_acc_list(const std::vector<float>& accs, float cdm) {
    XML::Element acc_trials("acceleration_trials");
    acc_trials.add_attribute("count", accs.size());
    acc_trials.add_attribute("DM", format_float_to_precision(cdm,4));
    for (size_t i=0; i<accs.size(); ++i) {
      XML::Element trial("trial");
      trial.add_attribute("id", i);
      trial.set_text(format_float_to_precision(accs[i],4));
      acc_trials.append(trial);
    }
    root.append(acc_trials);
  }

  void add_candidates(std::vector<Candidate>& candidates,
                      const std::map<unsigned,long int>& byte_map)
  {
    XML::Element cands("candidates");
    for (size_t i=0; i<candidates.size(); ++i) {
      const auto& C = candidates[i];
      XML::Element cand("candidate");
      cand.add_attribute("id", i);
      cand.append(XML::Element("period",          1.0/C.freq));
      cand.append(XML::Element("opt_period",      C.opt_period));
      cand.append(XML::Element("dm",              C.dm));
      cand.append(XML::Element("acc",             C.acc));
      cand.append(XML::Element("nh",              C.nh));
      cand.append(XML::Element("snr",             C.snr));
      cand.append(XML::Element("folded_snr",      C.folded_snr));
      cand.append(XML::Element("is_adjacent",     C.is_adjacent));
      cand.append(XML::Element("is_physical",     C.is_physical));
      cand.append(XML::Element("ddm_count_ratio", C.ddm_count_ratio));
      cand.append(XML::Element("ddm_snr_ratio",   C.ddm_snr_ratio));
      cand.append(XML::Element("nassoc",          C.count_assoc()));
      cand.append(XML::Element("byte_offset",     byte_map.at(i)));
      cands.append(cand);
    }
    root.append(cands);
  }

  void add_candidates( std::vector<Candidate>& candidates,
                      const std::map<int,std::string>& filenames)
  {
    XML::Element cands("candidates");
    for (size_t i=0; i<candidates.size(); ++i) {
      const auto& C = candidates[i];
      XML::Element cand("candidate");
      cand.add_attribute("id", i);
      cand.append(XML::Element("period",     1.0/C.freq));
      cand.append(XML::Element("opt_period", C.opt_period));
      cand.append(XML::Element("dm",         C.dm));
      cand.append(XML::Element("acc",        C.acc));
      cand.append(XML::Element("nh",         C.nh));
      cand.append(XML::Element("snr",        C.snr));
      cand.append(XML::Element("folded_snr", C.folded_snr));
      cand.append(XML::Element("is_adjacent",C.is_adjacent));
      cand.append(XML::Element("is_physical",C.is_physical));
      cand.append(XML::Element("ddm_count_ratio", C.ddm_count_ratio));
      cand.append(XML::Element("ddm_snr_ratio",   C.ddm_snr_ratio));
      cand.append(XML::Element("nassoc",          C.count_assoc()));
      cand.append(XML::Element("results_file",    filenames.at(i)));
      cands.append(cand);
    }
    root.append(cands);
  }
};

//-----------------------------------------------------------------------------
// Candidate binary dumper
//-----------------------------------------------------------------------------
class CandidateFileWriter {
public:
  std::map<int,std::string>        filenames;
  std::map<unsigned,long int>      byte_mapping;
  std::string                      output_dir;

  CandidateFileWriter(const std::string& outdir)
    : output_dir(outdir)
  {
    struct stat st;
    if (stat(output_dir.c_str(), &st) == -1)
      mkdir(output_dir.c_str(), 0777);
  }

  void write_binary(const std::vector<Candidate>& candidates,
                    const std::string& filename)
  {
    char actual[PATH_MAX];
    std::stringstream path;
    path << output_dir << "/" << filename;
    realpath(path.str().c_str(), actual);

    FILE* fo = fopen(actual, "w");
    if (!fo) { perror(path.str().c_str()); return; }
    for (size_t i=0; i<candidates.size(); ++i) {
      byte_mapping[i] = ftell(fo);
      if (!candidates[i].fold.empty()) {
        int nb = candidates[i].nbins, ni = candidates[i].nints;
        fprintf(fo,"FOLD");
        fwrite(&nb, sizeof(int),1,fo);
        fwrite(&ni, sizeof(int),1,fo);
        fwrite(&candidates[i].fold[0], sizeof(float), nb*ni, fo);
      }
      std::vector<CandidatePOD> dets;
      candidates[i].collect_candidates(dets);
      int nd = dets.size();
      fwrite(&nd, sizeof(int),1,fo);
      fwrite(&dets[0], sizeof(CandidatePOD), nd, fo);
    }
    fclose(fo);
  }

  


  
  void write_binaries(std::vector<Candidate>& candidates)
  {
    char actualpath [PATH_MAX];
    char filename[1024];
    std::stringstream filepath;
    for (int ii=0;ii<candidates.size();ii++){
      filepath.str("");
      sprintf(filename,"cand_%04d_%.5f_%.1f_%.1f.peasoup",
              ii,1.0/candidates[ii].freq,candidates[ii].dm,candidates[ii].acc);
      filepath << output_dir << "/" << filename;

      char* ptr = realpath(filepath.str().c_str(), actualpath);
      filenames[ii] = std::string(actualpath);
      
      FILE* fo = fopen(filepath.str().c_str(),"w");
      if (fo == NULL) {
	perror(filepath.str().c_str());
	return;
      }
      
      if (candidates[ii].fold.size()>0){
	size_t size = candidates[ii].nbins * candidates[ii].nints;
	float* fold = &candidates[ii].fold[0];
	fprintf(fo,"FOLD");
	fwrite(&candidates[ii].nbins,sizeof(int),1,fo);
	fwrite(&candidates[ii].nints,sizeof(int),1,fo);
	fwrite(fold,sizeof(float),size,fo);
      }
      std::vector<CandidatePOD> detections;
      candidates[ii].collect_candidates(detections);
      int ndets = detections.size();
      fwrite(&ndets,sizeof(int),1,fo);
      fwrite(&detections[0],sizeof(CandidatePOD),ndets,fo);
      fclose(fo);
    }
  }
};

