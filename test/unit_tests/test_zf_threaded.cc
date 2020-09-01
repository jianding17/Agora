#include <gtest/gtest.h>
// For some reason, gtest include order matters
#include "concurrentqueue.h"
#include "config.hpp"
#include "dozf.hpp"
#include "gettime.h"
#include "utils.h"
#include <thread>

static constexpr size_t kNumWorkers = 14;
static constexpr size_t kMaxTestNum = 100;
static constexpr size_t kMaxItrNum = (1 << 30);
static constexpr size_t kAntTestNum = 3;
static constexpr size_t bs_ant_nums[kAntTestNum] = { 32, 16, 48 };
static constexpr size_t frame_offsets[kAntTestNum] = { 0, 20, 30 };
// A spinning barrier to synchronize the start of worker threads
std::atomic<size_t> num_workers_ready_atomic;

void MasterToWorkerDynamic_master(Config* cfg,
    moodycamel::ConcurrentQueue<Event_data>& event_queue,
    moodycamel::ConcurrentQueue<Event_data>& complete_task_queue)
{
    pin_to_core_with_offset(ThreadType::kMaster, cfg->core_offset, 0);
    // Wait for all worker threads to be ready
    while (num_workers_ready_atomic != kNumWorkers) {
        // Wait
    }

    for (size_t bs_ant_idx = 0; bs_ant_idx < kAntTestNum; bs_ant_idx++) {
        cfg->BS_ANT_NUM = bs_ant_nums[bs_ant_idx];
        for (size_t i = 0; i < kMaxTestNum; i++) {
            uint32_t frame_id
                = i / cfg->zf_events_per_symbol + frame_offsets[bs_ant_idx];
            size_t base_sc_id
                = (i % cfg->zf_events_per_symbol) * cfg->zf_block_size;
            event_queue.enqueue(Event_data(
                EventType::kZF, gen_tag_t::frm_sc(frame_id, base_sc_id)._tag));
        }

        // Dequeue all events in queue to avoid overflow
        size_t num_finished_events = 0;
        while (num_finished_events < kMaxTestNum) {
            Event_data event;
            int ret = complete_task_queue.try_dequeue(event);
            if (ret)
                num_finished_events++;
        }
    }
}

void MasterToWorkerDynamic_worker(Config* cfg, size_t worker_id,
    double freq_ghz, moodycamel::ConcurrentQueue<Event_data>& event_queue,
    moodycamel::ConcurrentQueue<Event_data>& complete_task_queue,
    moodycamel::ProducerToken* ptok,
    PMat2D<TASK_BUFFER_FRAME_NUM, kMaxUEs, complex_float> csi_buffers,
    Table<complex_float>& recip_buffer,
    PMat2D<kFrameWnd, kMaxDataSCs, complex_float> ul_zf_matrices,
    PMat2D<kFrameWnd, kMaxDataSCs, complex_float> dl_zf_matrices, Stats* stats)
{
    pin_to_core_with_offset(
        ThreadType::kWorker, cfg->core_offset + 1, worker_id);

    // Wait for all threads (including master) to start runnung
    num_workers_ready_atomic++;
    while (num_workers_ready_atomic != kNumWorkers) {
        // Wait
    }

    auto computeZF = new DoZF(cfg, worker_id, freq_ghz, event_queue,
        complete_task_queue, ptok, csi_buffers, recip_buffer, ul_zf_matrices,
        dl_zf_matrices, stats);

    size_t start_tsc = rdtsc();
    size_t num_tasks = 0;
    Event_data req_event;
    size_t max_frame_id_wo_offset
        = (kMaxTestNum - 1) / (cfg->OFDM_DATA_NUM / cfg->zf_block_size);
    for (size_t i = 0; i < kMaxItrNum; i++) {
        if (event_queue.try_dequeue(req_event)) {
            num_tasks++;
            size_t frame_offset_id = 0;
            size_t cur_frame_id = gen_tag_t(req_event.tags[0]).frame_id;
            if (cur_frame_id >= frame_offsets[1]
                and cur_frame_id - frame_offsets[1] <= max_frame_id_wo_offset) {
                frame_offset_id = 1;
            } else if (cur_frame_id >= frame_offsets[2]
                and cur_frame_id - frame_offsets[2] <= max_frame_id_wo_offset) {
                frame_offset_id = 2;
            }
            ASSERT_EQ(cfg->BS_ANT_NUM, bs_ant_nums[frame_offset_id]);
            Event_data resp_event = computeZF->launch(req_event.tags[0]);
            try_enqueue_fallback(&complete_task_queue, ptok, resp_event);
        }
    }
    double ms = cycles_to_ms(rdtsc() - start_tsc, freq_ghz);

    printf("Worker %zu: %zu tasks, time per task = %.4f ms\n", worker_id,
        num_tasks, ms / num_tasks);
}

/// Test correctness of BS_ANT_NUM values in multi-threaded zeroforcing
/// when BS_ANT_NUM varies in runtime
TEST(TestZF, VaryingConfig)
{
    static constexpr size_t kNumIters = 10000;
    auto* cfg = new Config("data/tddconfig-sim-ul.json");
    cfg->genData();

    double freq_ghz = measure_rdtsc_freq();

    auto event_queue = moodycamel::ConcurrentQueue<Event_data>(2 * kNumIters);
    moodycamel::ProducerToken* ptoks[kNumWorkers];
    auto complete_task_queue
        = moodycamel::ConcurrentQueue<Event_data>(2 * kNumIters);
    for (size_t i = 0; i < kNumWorkers; i++) {
        ptoks[i] = new moodycamel::ProducerToken(complete_task_queue);
    }

    Table<complex_float> recip_buffer;

    PMat2D<TASK_BUFFER_FRAME_NUM, kMaxUEs, complex_float> csi_buffers;
    csi_buffers.rand_alloc_cx_float(cfg->BS_ANT_NUM * cfg->OFDM_DATA_NUM);

    PMat2D<kFrameWnd, kMaxDataSCs, complex_float> ul_zf_matrices(
        cfg->BS_ANT_NUM * cfg->UE_NUM);
    PMat2D<kFrameWnd, kMaxDataSCs, complex_float> dl_zf_matrices(
        cfg->UE_NUM * cfg->BS_ANT_NUM);

    recip_buffer.rand_alloc_cx_float(
        TASK_BUFFER_FRAME_NUM, kMaxDataSCs * kMaxAntennas, 64);

    auto stats = new Stats(cfg, kMaxStatBreakdown, freq_ghz);

    auto master = std::thread(MasterToWorkerDynamic_master, cfg,
        std::ref(event_queue), std::ref(complete_task_queue));
    std::thread workers[kNumWorkers];
    for (size_t i = 0; i < kNumWorkers; i++) {
        workers[i] = std::thread(MasterToWorkerDynamic_worker, cfg, i, freq_ghz,
            std::ref(event_queue), std::ref(complete_task_queue), ptoks[i],
            std::ref(csi_buffers), std::ref(recip_buffer),
            std::ref(ul_zf_matrices), std::ref(dl_zf_matrices), stats);
    }
    master.join();
    for (auto& w : workers)
        w.join();
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
