#include "stats.hpp"
#include <typeinfo>

Stats::Stats(Config* cfg)
    : config_(cfg)
    , task_thread_num(cfg->worker_thread_num)
    , fft_thread_num(cfg->fft_thread_num)
    , zf_thread_num(cfg->zf_thread_num)
    , demul_thread_num(cfg->demul_thread_num)
    , decode_thread_num(cfg->decode_thread_num)
    , freq_ghz(cfg->freq_ghz())
    , creation_tsc(rdtsc())
{
    frame_start.calloc(config_->socket_thread_num, kNumStatsFrames,
        Agora_memory::Alignment_t::k64Align);
}

Stats::~Stats() {}

void Stats::populate_summary(
    FrameSummary* frame_summary, size_t thread_id, DoerType doer_type)
{
    DurationStat* ds = get_duration_stat(doer_type, thread_id);
    DurationStat* ds_old = get_duration_stat_old(doer_type, thread_id);

    frame_summary->count_this_thread = ds->task_count - ds_old->task_count;
    frame_summary->count_all_threads += frame_summary->count_this_thread;

    for (size_t j = 0; j < break_down_num; j++) {
        frame_summary->us_this_thread[j] = cycles_to_us(
            ds->task_duration[j] - ds_old->task_duration[j], freq_ghz);
        frame_summary->us_avg_threads[j] += frame_summary->us_this_thread[j];
    }
    *ds_old = *ds;
}

void Stats::compute_avg_over_threads(
    FrameSummary* frame_summary, size_t thread_num, size_t break_down_num)
{
    for (size_t j = 0; j < break_down_num; j++)
        frame_summary->us_avg_threads[j]
            = frame_summary->us_avg_threads[j] / thread_num;
}

void Stats::print_per_thread_per_task(FrameSummary s)
{
    std::printf("%zu tasks %.1f us (~ %.1f + %.1f + %.1f us), ",
        s.count_this_thread, s.us_this_thread[0] / s.count_this_thread,
        s.us_this_thread[1] / s.count_this_thread,
        s.us_this_thread[2] / s.count_this_thread,
        s.us_this_thread[3] / s.count_this_thread);
}

void Stats::print_per_frame(const char* doer_string, FrameSummary frame_summary)
{
    std::printf("%s (%zu tasks): %.3f ms (~ %.4f + %.3f + %.4f ms), ",
        doer_string, frame_summary.count_all_threads,
        frame_summary.us_avg_threads[0] / 1000.0,
        frame_summary.us_avg_threads[1] / 1000.0,
        frame_summary.us_avg_threads[2] / 1000.0,
        frame_summary.us_avg_threads[3] / 1000.0);
}

void Stats::update_stats_in_functions_uplink(size_t frame_id)
{
    this->last_frame_id_ = frame_id;
    size_t frame_slot = frame_id % kNumStatsFrames;
    if (kIsWorkerTimingEnabled == false) {
        return;
    }

    FrameSummary fft_frame_summary;
    FrameSummary csi_frame_summary;
    FrameSummary zf_frame_summary;
    FrameSummary demul_frame_summary;
    FrameSummary decode_frame_summary;

    if (config_->bigstation_mode)
        update_stats_in_functions_uplink_bigstation(frame_slot,
            &fft_frame_summary, &csi_frame_summary, &zf_frame_summary,
            &demul_frame_summary, &decode_frame_summary);
    else
        update_stats_in_functions_uplink_agora(frame_slot, &fft_frame_summary,
            &csi_frame_summary, &zf_frame_summary, &demul_frame_summary,
            &decode_frame_summary);

    fft_us[frame_slot] = fft_frame_summary.us_avg_threads[0];
    csi_us[frame_slot] = csi_frame_summary.us_avg_threads[0];
    zf_us[frame_slot] = zf_frame_summary.us_avg_threads[0];
    demul_us[frame_slot] = demul_frame_summary.us_avg_threads[0];
    decode_us[frame_slot] = decode_frame_summary.us_avg_threads[0];

    const double sum_us_this_frame = fft_us[frame_slot] + csi_us[frame_slot]
        + zf_us[frame_slot] + demul_us[frame_slot] + decode_us[frame_slot];

    for (size_t i = 1; i < break_down_num; i++) {
        fft_breakdown_us[i - 1][frame_slot]
            = fft_frame_summary.us_avg_threads[i];
        csi_breakdown_us[i - 1][frame_slot]
            = csi_frame_summary.us_avg_threads[i];
        zf_breakdown_us[i - 1][frame_slot] = zf_frame_summary.us_avg_threads[i];
        demul_breakdown_us[i - 1][frame_slot]
            = demul_frame_summary.us_avg_threads[i];
        decode_breakdown_us[i - 1][frame_slot]
            = decode_frame_summary.us_avg_threads[i];
    }

    if (kStatsPrintFrameSummary) {
        std::printf("Frame %zu summary: ", frame_id);
        print_per_frame("FFT", fft_frame_summary);
        print_per_frame("CSI", csi_frame_summary);
        print_per_frame("ZF", zf_frame_summary);
        print_per_frame("Demul", demul_frame_summary);
        print_per_frame("Decode", decode_frame_summary);
        std::printf("Total: %.2f ms\n", sum_us_this_frame / 1000);
    }
}

void Stats::update_stats_in_functions_downlink(size_t frame_id)
{
    this->last_frame_id_ = frame_id;
    size_t frame_slot = frame_id % kNumStatsFrames;
    if (kIsWorkerTimingEnabled == false) {
        return;
    }

    FrameSummary ifft_frame_summary;
    FrameSummary csi_frame_summary;
    FrameSummary zf_frame_summary;
    FrameSummary precode_frame_summary;
    FrameSummary encode_frame_summary;

    if (config_->bigstation_mode)
        update_stats_in_functions_downlink_bigstation(frame_slot,
            &ifft_frame_summary, &csi_frame_summary, &zf_frame_summary,
            &precode_frame_summary, &encode_frame_summary);
    else
        update_stats_in_functions_downlink_agora(frame_slot,
            &ifft_frame_summary, &csi_frame_summary, &zf_frame_summary,
            &precode_frame_summary, &encode_frame_summary);

    csi_us[frame_slot] = csi_frame_summary.us_avg_threads[0];
    ifft_us[frame_slot] = ifft_frame_summary.us_avg_threads[0];
    zf_us[frame_slot] = zf_frame_summary.us_avg_threads[0];
    precode_us[frame_slot] = precode_frame_summary.us_avg_threads[0];
    encode_us[frame_slot] = encode_frame_summary.us_avg_threads[0];

    const double sum_us_this_frame = csi_us[frame_slot] + ifft_us[frame_slot]
        + zf_us[frame_slot] + precode_us[frame_slot] + encode_us[frame_slot];

    if (kStatsPrintFrameSummary) {
        std::printf("Frame %zu summary: ", frame_id);
        print_per_frame("CSI", csi_frame_summary);
        print_per_frame("IFFT", ifft_frame_summary);
        print_per_frame("ZF", zf_frame_summary);
        print_per_frame("Precode", precode_frame_summary);
        print_per_frame("Encode", encode_frame_summary);
        std::printf("Total: %.2f ms\n", sum_us_this_frame / 1000.0);
    }
}

void Stats::update_stats_in_dofft_bigstation(size_t frame_id, size_t thread_num,
    size_t thread_num_offset, FrameSummary* fft_frame_summary,
    FrameSummary* csi_frame_summary)
{
    for (size_t i = thread_num_offset; i < thread_num_offset + thread_num;
         i++) {
        populate_summary(fft_frame_summary, i, DoerType::kFFT);
        populate_summary(csi_frame_summary, i, DoerType::kCSI);

        if (kDebugPrintStatsPerThread) {
            double sum_us_this_frame_this_thread
                = fft_frame_summary->us_this_thread[0]
                + csi_frame_summary->us_this_thread[0];
            std::printf("In frame %zu, thread %zu, \t", frame_id, i);
            std::printf("csi: ");
            print_per_thread_per_task(*csi_frame_summary);
            std::printf("fft: ");
            print_per_thread_per_task(*fft_frame_summary);
            std::printf("sum: %.3f us\n", sum_us_this_frame_this_thread);
        }
    }
    compute_avg_over_threads(fft_frame_summary, thread_num, break_down_num);
    compute_avg_over_threads(csi_frame_summary, thread_num, break_down_num);
}

void Stats::update_stats_in_dozf_bigstation(size_t frame_id, size_t thread_num,
    size_t thread_num_offset, FrameSummary* zf_frame_summary)
{
    for (size_t i = thread_num_offset; i < thread_num_offset + thread_num;
         i++) {
        populate_summary(zf_frame_summary, i, DoerType::kZF);

        if (kDebugPrintStatsPerThread) {
            double sum_us_this_frame_this_thread
                = zf_frame_summary->us_this_thread[0];
            std::printf("In frame %zu, thread %zu, \t", frame_id, i);
            std::printf("zf: ");
            print_per_thread_per_task(*zf_frame_summary);
            std::printf("sum: %.3f us\n", sum_us_this_frame_this_thread);
        }
    }
    compute_avg_over_threads(zf_frame_summary, thread_num, break_down_num);
}

void Stats::update_stats_in_dodemul_bigstation(size_t frame_id,
    size_t thread_num, size_t thread_num_offset,
    FrameSummary* demul_frame_summary)
{
    for (size_t i = thread_num_offset; i < thread_num_offset + thread_num;
         i++) {
        populate_summary(demul_frame_summary, i, DoerType::kDemul);

        if (kDebugPrintStatsPerThread) {
            double sum_us_this_frame_this_thread
                = demul_frame_summary->us_this_thread[0];
            std::printf("In frame %zu, thread %zu, \t", frame_id, i);
            std::printf("demul: ");
            print_per_thread_per_task(*demul_frame_summary);
            std::printf("sum: %.3f us\n", sum_us_this_frame_this_thread);
        }
    }
    compute_avg_over_threads(demul_frame_summary, thread_num, break_down_num);
}

void Stats::update_stats_in_dodecode_bigstation(size_t frame_id,
    size_t thread_num, size_t thread_num_offset,
    FrameSummary* decode_frame_summary)
{
    for (size_t i = thread_num_offset; i < thread_num_offset + thread_num;
         i++) {
        populate_summary(decode_frame_summary, i, DoerType::kDecode);

        if (kDebugPrintStatsPerThread) {
            double sum_us_this_frame_this_thread
                = decode_frame_summary->us_this_thread[0];
            std::printf("In frame %zu, thread %zu, \t", frame_id, i);
            std::printf("decode: ");
            print_per_thread_per_task(*decode_frame_summary);
            std::printf("sum: %.3f us\n", sum_us_this_frame_this_thread);
        }
    }
    compute_avg_over_threads(decode_frame_summary, thread_num, break_down_num);
}

void Stats::update_stats_in_doifft_bigstation(size_t frame_id,
    size_t thread_num, size_t thread_num_offset,
    FrameSummary* ifft_frame_summary, FrameSummary* csi_frame_summary)
{
    for (size_t i = thread_num_offset; i < thread_num_offset + thread_num;
         i++) {
        populate_summary(ifft_frame_summary, i, DoerType::kIFFT);
        populate_summary(csi_frame_summary, i, DoerType::kCSI);

        if (kDebugPrintStatsPerThread) {
            double sum_time_this_frame_this_thread
                = ifft_frame_summary->us_this_thread[0]
                + csi_frame_summary->us_this_thread[0];
            std::printf("In frame %zu, thread %zu, \t", frame_id, i);
            std::printf("csi: ");
            print_per_thread_per_task(*csi_frame_summary);
            std::printf("ifft: ");
            print_per_thread_per_task(*ifft_frame_summary);
            std::printf("sum: %.3f us\n", sum_time_this_frame_this_thread);
        }
    }
    compute_avg_over_threads(ifft_frame_summary, thread_num, break_down_num);
    compute_avg_over_threads(csi_frame_summary, thread_num, break_down_num);
}

void Stats::update_stats_in_doprecode_bigstation(size_t frame_id,
    size_t thread_num, size_t thread_num_offset,
    FrameSummary* precode_frame_summary)
{
    for (size_t i = thread_num_offset; i < thread_num_offset + thread_num;
         i++) {
        populate_summary(precode_frame_summary, i, DoerType::kPrecode);

        if (kDebugPrintStatsPerThread) {
            double sum_us_this_frame_this_thread
                = precode_frame_summary->us_this_thread[0];
            std::printf("In frame %zu, thread %zu, \t", frame_id, i);
            std::printf("precode: ");
            print_per_thread_per_task(*precode_frame_summary);
            std::printf("sum: %.3f\n", sum_us_this_frame_this_thread);
        }
    }
    compute_avg_over_threads(precode_frame_summary, thread_num, break_down_num);
}

void Stats::update_stats_in_doencode_bigstation(size_t frame_id,
    size_t thread_num, size_t thread_num_offset,
    FrameSummary* encode_frame_summary)
{
    for (size_t i = thread_num_offset; i < thread_num_offset + thread_num;
         i++) {
        populate_summary(encode_frame_summary, i, DoerType::kEncode);

        if (kDebugPrintStatsPerThread) {
            double sum_us_this_frame_this_thread
                = encode_frame_summary->us_this_thread[0];
            std::printf("In frame %zu, thread %zu, \t", frame_id, i);
            std::printf("precode: ");
            print_per_thread_per_task(*encode_frame_summary);
            std::printf("sum: %.3f\n", sum_us_this_frame_this_thread);
        }
    }
    compute_avg_over_threads(encode_frame_summary, thread_num, break_down_num);
}

void Stats::update_stats_in_functions_uplink_bigstation(size_t frame_id,
    FrameSummary* fft_frame_summary, FrameSummary* csi_frame_summary,
    FrameSummary* zf_frame_summary, FrameSummary* demul_frame_summary,
    FrameSummary* decode_frame_summary)
{
    update_stats_in_dofft_bigstation(
        frame_id, fft_thread_num, 0, fft_frame_summary, csi_frame_summary);
    update_stats_in_dozf_bigstation(
        frame_id, zf_thread_num, fft_thread_num, zf_frame_summary);
    update_stats_in_dodemul_bigstation(frame_id, demul_thread_num,
        fft_thread_num + zf_thread_num, demul_frame_summary);
    update_stats_in_dodecode_bigstation(frame_id, decode_thread_num,
        fft_thread_num + zf_thread_num + demul_thread_num,
        decode_frame_summary);
}

void Stats::update_stats_in_functions_downlink_bigstation(size_t frame_id,
    FrameSummary* ifft_frame_summary, FrameSummary* csi_frame_summary,
    FrameSummary* zf_frame_summary, FrameSummary* precode_frame_summary,
    FrameSummary* encode_frame_summary)
{
    update_stats_in_doifft_bigstation(
        frame_id, fft_thread_num, 0, ifft_frame_summary, csi_frame_summary);
    update_stats_in_dozf_bigstation(
        frame_id, zf_thread_num, fft_thread_num, zf_frame_summary);
    update_stats_in_doprecode_bigstation(frame_id, demul_thread_num,
        fft_thread_num + zf_thread_num, precode_frame_summary);
    update_stats_in_doencode_bigstation(frame_id, decode_thread_num,
        fft_thread_num + zf_thread_num + demul_thread_num,
        encode_frame_summary);
}

void Stats::update_stats_in_functions_uplink_agora(size_t frame_id,
    FrameSummary* fft_frame_summary, FrameSummary* csi_frame_summary,
    FrameSummary* zf_frame_summary, FrameSummary* demul_frame_summary,
    FrameSummary* decode_frame_summary)
{
    for (size_t i = 0; i < task_thread_num; i++) {
        populate_summary(fft_frame_summary, i, DoerType::kFFT);
        populate_summary(csi_frame_summary, i, DoerType::kCSI);
        populate_summary(zf_frame_summary, i, DoerType::kZF);
        populate_summary(demul_frame_summary, i, DoerType::kDemul);
        populate_summary(decode_frame_summary, i, DoerType::kDecode);

        if (kDebugPrintStatsPerThread) {
            double sum_us_this_frame_this_thread
                = fft_frame_summary->us_this_thread[0]
                + csi_frame_summary->us_this_thread[0]
                + zf_frame_summary->us_this_thread[0]
                + demul_frame_summary->us_this_thread[0]
                + decode_frame_summary->us_this_thread[0];
            std::printf("In frame %zu, thread %zu, \t", frame_id, i);
            std::printf("csi: ");
            print_per_thread_per_task(*csi_frame_summary);
            std::printf("fft: ");
            print_per_thread_per_task(*fft_frame_summary);
            std::printf("zf: ");
            print_per_thread_per_task(*zf_frame_summary);
            std::printf("demul: ");
            print_per_thread_per_task(*demul_frame_summary);
            std::printf("decode: ");
            print_per_thread_per_task(*decode_frame_summary);
            std::printf("sum: %.3f\n", sum_us_this_frame_this_thread);
        }
    }
    compute_avg_over_threads(
        fft_frame_summary, task_thread_num, break_down_num);
    compute_avg_over_threads(
        csi_frame_summary, task_thread_num, break_down_num);
    compute_avg_over_threads(zf_frame_summary, task_thread_num, break_down_num);
    compute_avg_over_threads(
        demul_frame_summary, task_thread_num, break_down_num);
    compute_avg_over_threads(
        decode_frame_summary, task_thread_num, break_down_num);
}

void Stats::update_stats_in_functions_downlink_agora(size_t frame_id,
    FrameSummary* ifft_frame_summary, FrameSummary* csi_frame_summary,
    FrameSummary* zf_frame_summary, FrameSummary* precode_frame_summary,
    FrameSummary* encode_frame_summary)
{

    for (size_t i = 0; i < task_thread_num; i++) {
        populate_summary(ifft_frame_summary, i, DoerType::kIFFT);
        populate_summary(csi_frame_summary, i, DoerType::kCSI);
        populate_summary(zf_frame_summary, i, DoerType::kZF);
        populate_summary(precode_frame_summary, i, DoerType::kPrecode);
        populate_summary(encode_frame_summary, i, DoerType::kEncode);

        if (kDebugPrintStatsPerThread) {
            double sum_us_this_frame_this_thread
                = ifft_frame_summary->us_this_thread[0]
                + csi_frame_summary->us_this_thread[0]
                + zf_frame_summary->us_this_thread[0]
                + precode_frame_summary->us_this_thread[0]
                + encode_frame_summary->us_this_thread[0];
            std::printf("In frame %zu, thread %zu, \t", frame_id, i);
            std::printf("csi: ");
            print_per_thread_per_task(*csi_frame_summary);
            std::printf("ifft: ");
            print_per_thread_per_task(*ifft_frame_summary);
            std::printf("zf: ");
            print_per_thread_per_task(*zf_frame_summary);
            std::printf("precode: ");
            print_per_thread_per_task(*precode_frame_summary);
            std::printf("encode: ");
            print_per_thread_per_task(*encode_frame_summary);
            std::printf("sum: %.3f us\n", sum_us_this_frame_this_thread);
        }
    }
    compute_avg_over_threads(
        ifft_frame_summary, task_thread_num, break_down_num);
    compute_avg_over_threads(
        csi_frame_summary, task_thread_num, break_down_num);
    compute_avg_over_threads(zf_frame_summary, task_thread_num, break_down_num);
    compute_avg_over_threads(
        precode_frame_summary, task_thread_num, break_down_num);
    compute_avg_over_threads(
        encode_frame_summary, task_thread_num, break_down_num);
}

void Stats::save_to_file()
{
    std::string cur_directory = TOSTRING(PROJECT_DIRECTORY);
    std::string filename = cur_directory + "/data/timeresult.txt";
    std::printf("Stats: Saving master timestamps to %s\n", filename.c_str());
    FILE* fp_debug = std::fopen(filename.c_str(), "w");
    rt_assert(fp_debug != nullptr,
        std::string("Open file failed ") + std::to_string(errno));

    if ( config_->frame().NumDLSyms() > 0 ) {
        std::fprintf(fp_debug,
            "Pilot RX by socket threads (= reference time), "
            "kPilotRX, kProcessingStarted, kPilotAllRX, kFFTPilotsDone, "
            "kZFDone, kPrecodeDone, kIFFTDone, kEncodeDone, kRXDone\n");

        for (size_t i = 0; i < this->last_frame_id_; i++) {
            size_t ref_tsc = SIZE_MAX;
            for (size_t j = 0; j < config_->socket_thread_num; j++) {
                ref_tsc = std::min(ref_tsc, frame_start[j][i]);
            }
            std::fprintf(fp_debug,
                "%.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f \n",
                cycles_to_us(ref_tsc - creation_tsc, freq_ghz),
                master_get_us_from_ref(TsType::kPilotRX, i, ref_tsc),
                master_get_us_from_ref(TsType::kProcessingStarted, i, ref_tsc),
                master_get_us_from_ref(TsType::kPilotAllRX, i, ref_tsc),
                master_get_us_from_ref(TsType::kFFTPilotsDone, i, ref_tsc),
                master_get_us_from_ref(TsType::kZFDone, i, ref_tsc),
                master_get_us_from_ref(TsType::kPrecodeDone, i, ref_tsc),
                master_get_us_from_ref(TsType::kIFFTDone, i, ref_tsc),
                master_get_us_from_ref(TsType::kEncodeDone, i, ref_tsc),
                master_get_us_from_ref(TsType::kRXDone, i, ref_tsc));
        }
    } else {
        // Print the header
        std::fprintf(fp_debug,
            "Pilot RX by socket threads (= reference time), "
            "kPilotRX, kProcessingStarted, kPilotAllRX, kFFTPilotsDone, "
            "kZFDone, kDemulDone, kDecodeDone, kRXDone, time in CSI, time in "
            "FFT, time in ZF, time in Demul, time in Decode\n");
        for (size_t i = 0; i < this->last_frame_id_; i++) {
            size_t ref_tsc = SIZE_MAX;
            for (size_t j = 0; j < config_->socket_thread_num; j++) {
                ref_tsc = std::min(ref_tsc, frame_start[j][i]);
            }

            std::fprintf(fp_debug,
                "%.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.3f %.3f %.3f "
                "%.3f %.3f\n",
                cycles_to_us(ref_tsc - creation_tsc, freq_ghz),
                master_get_us_from_ref(TsType::kPilotRX, i, ref_tsc),
                master_get_us_from_ref(TsType::kProcessingStarted, i, ref_tsc),
                master_get_us_from_ref(TsType::kPilotAllRX, i, ref_tsc),
                master_get_us_from_ref(TsType::kFFTPilotsDone, i, ref_tsc),
                master_get_us_from_ref(TsType::kZFDone, i, ref_tsc),
                master_get_us_from_ref(TsType::kDemulDone, i, ref_tsc),
                master_get_us_from_ref(TsType::kDecodeDone, i, ref_tsc),
                master_get_us_from_ref(TsType::kRXDone, i, ref_tsc), csi_us[i],
                fft_us[i], zf_us[i], demul_us[i], decode_us[i]);
        }
    }

    std::fclose(fp_debug);

    if (kIsWorkerTimingEnabled) {
        std::string filename_detailed
            = cur_directory + "/data/timeresult_detail.txt";
        std::printf("Stats: Printing detailed results to %s\n",
            filename_detailed.c_str());

        FILE* fp_debug_detailed = std::fopen(filename_detailed.c_str(), "w");
        rt_assert(fp_debug_detailed != nullptr,
            std::string("Open file failed ") + std::to_string(errno));
        // Print the header
        std::fprintf(fp_debug_detailed,
            "fft_0, fft_1, fft_2, zf_0, zf_1, zf_2, demul_0, demul_1, demul_2, "
            "decode_0, decode_1, decode_2\n");

        for (size_t i = 0; i < this->last_frame_id_; i++) {
            std::fprintf(fp_debug_detailed,
                "%.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f\n",
                fft_breakdown_us[0][i] + csi_breakdown_us[0][i],
                fft_breakdown_us[1][i] + csi_breakdown_us[1][i],
                fft_breakdown_us[2][i] + csi_breakdown_us[2][i],
                zf_breakdown_us[0][i], zf_breakdown_us[1][i],
                zf_breakdown_us[2][i], demul_breakdown_us[0][i],
                demul_breakdown_us[1][i], demul_breakdown_us[2][i],
                decode_breakdown_us[0][i], decode_breakdown_us[1][i],
                decode_breakdown_us[2][i]);
        }
        std::fclose(fp_debug_detailed);
    }
}

size_t Stats::get_total_task_count(DoerType doer_type, size_t thread_num)
{
    size_t total_count = 0;
    for (size_t i = 0; i < thread_num; i++) {
        total_count = total_count + get_duration_stat(doer_type, i)->task_count;
    }
    return total_count;
}

void Stats::print_summary( void )
{
    std::printf("Stats: total processed frames %zu\n", this->last_frame_id_ + 1);
    if (kIsWorkerTimingEnabled == false) {
        std::printf("Stats: Worker timing is disabled. Not printing summary\n");
        return;
    }

    size_t num_csi_tasks
        = get_total_task_count(DoerType::kCSI, task_thread_num);
    size_t num_fft_tasks
        = get_total_task_count(DoerType::kFFT, task_thread_num);
    size_t num_zf_tasks = get_total_task_count(DoerType::kZF, task_thread_num);
    size_t num_demul_tasks
        = get_total_task_count(DoerType::kDemul, task_thread_num);
    size_t num_decode_tasks
        = get_total_task_count(DoerType::kDecode, task_thread_num);
    size_t num_encode_tasks
        = get_total_task_count(DoerType::kEncode, task_thread_num);
    size_t num_ifft_tasks
        = get_total_task_count(DoerType::kIFFT, task_thread_num);
    size_t num_precode_tasks
        = get_total_task_count(DoerType::kPrecode, task_thread_num);
    double csi_frames
        = (double)num_csi_tasks / this->config_->bs_ant_num() / this->config_->frame().NumPilotSyms();
    double zf_frames = (double)num_zf_tasks / this->config_->zf_events_per_symbol;

    if (config_->frame().NumDLSyms() > 0) {
        double precode_frames = (double)num_precode_tasks / this->config_->ofdm_data_num()
            / this->config_->frame().NumDLSyms();
        double ifft_frames = (double)num_ifft_tasks / this->config_->bs_ant_num()
            / this->config_->frame().NumDLSyms();
        double encode_frames = (double)num_encode_tasks
            / this->config_->ldpc_config().num_blocks_in_symbol() / this->config_->ue_num()
            / this->config_->frame().NumDLSyms();
        std::printf("Downlink totals (tasks, frames): ");
        std::printf("CSI (%zu, %.2f), ", num_csi_tasks, csi_frames);
        std::printf("ZF (%zu, %.2f), ", num_zf_tasks, zf_frames);
        std::printf("Encode (%zu, %.2f), ", num_encode_tasks, encode_frames);
        std::printf("Precode (%zu, %.2f), ", num_precode_tasks, precode_frames);
        std::printf("IFFT (%zu, %.2f)", num_ifft_tasks, ifft_frames);
        std::printf("\n");
        for (size_t i = 0; i < task_thread_num; i++) {
            size_t num_csi_i = get_duration_stat(DoerType::kCSI, i)->task_count;
            size_t num_zf_i = get_duration_stat(DoerType::kZF, i)->task_count;
            size_t num_precode_i
                = get_duration_stat(DoerType::kPrecode, i)->task_count;
            size_t num_ifft_i
                = get_duration_stat(DoerType::kIFFT, i)->task_count;
            size_t num_encode_i
                = get_duration_stat(DoerType::kEncode, i)->task_count;

            double percent_csi = num_csi_i * 100.0 / num_csi_tasks;
            double percent_zf = num_zf_i * 100.0 / num_zf_tasks;
            double percent_precode = num_precode_i * 100.0 / num_precode_tasks;
            double percent_ifft = num_ifft_i * 100.0 / num_ifft_tasks;
            double percent_encode = num_encode_i * 100.0 / num_encode_tasks;
            std::printf("Thread %zu performed (tasks, fraction of tasks): ", i);
            std::printf("CSI (%zu, %.2f%%), ", num_csi_i, percent_csi);
            std::printf("ZF (%zu, %.2f%%), ", num_zf_i, percent_zf);
            std::printf("Encode (%zu, %.2f%%), ", num_encode_i, percent_encode);
            std::printf(
                "Precode (%zu, %.2f%%), ", num_precode_i, percent_precode);
            std::printf("IFFT (%zu, %.2f%%)", num_ifft_i, percent_ifft);
            std::printf("\n");
        }
    } else {
        double fft_frames = (double)num_fft_tasks / this->config_->bs_ant_num()
            / this->config_->frame().NumULSyms();
        double demul_frames = (double)num_demul_tasks / this->config_->ofdm_data_num()
            / this->config_->frame().NumULSyms();
        double decode_frames = (double)num_decode_tasks
            / this->config_->ldpc_config().num_blocks_in_symbol() / this->config_->ue_num()
            / this->config_->frame().NumULSyms();
        std::printf("Uplink totals (tasks, frames): ");
        std::printf("CSI (%zu, %.2f), ", num_csi_tasks, csi_frames);
        std::printf("ZF (%zu, %.2f), ", num_zf_tasks, zf_frames);
        std::printf("FFT (%zu, %.2f), ", num_fft_tasks, fft_frames);
        std::printf("Demul (%zu, %.2f), ", num_demul_tasks, demul_frames);
        std::printf("Decode (%zu, %.2f)", num_decode_tasks, decode_frames);
        std::printf("\n");
        for (size_t i = 0; i < task_thread_num; i++) {
            size_t num_csi_i = get_duration_stat(DoerType::kCSI, i)->task_count;
            size_t num_fft_i = get_duration_stat(DoerType::kFFT, i)->task_count;
            size_t num_zf_i = get_duration_stat(DoerType::kZF, i)->task_count;
            size_t num_demul_i
                = get_duration_stat(DoerType::kDemul, i)->task_count;
            size_t num_decode_i
                = get_duration_stat(DoerType::kDecode, i)->task_count;

            double percent_csi = num_csi_i * 100.0 / num_csi_tasks;
            double percent_fft = num_fft_i * 100.0 / num_fft_tasks;
            double percent_zf = num_zf_i * 100.0 / num_zf_tasks;
            double percent_demul = num_demul_i * 100.0 / num_demul_tasks;
            double percent_decode = num_decode_i * 100.0 / num_decode_tasks;

            std::printf("Thread %zu performed (tasks, fraction of tasks): ", i);
            std::printf("CSI (%zu, %.1f%%), ", num_csi_i, percent_csi);
            std::printf("ZF (%zu, %.1f%%), ", num_zf_i, percent_zf);
            std::printf("FFT (%zu, %.1f%%), ", num_fft_i, percent_fft);
            std::printf("Demul (%zu, %.1f%%), ", num_demul_i, percent_demul);
            std::printf("Decode (%zu, %.1f%%) ", num_decode_i, percent_decode);
            std::printf("\n");
        }
    }
}
