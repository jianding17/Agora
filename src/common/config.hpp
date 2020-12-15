#ifndef CONFIG_HEADER
#define CONFIG_HEADER

#include <boost/range/algorithm/count.hpp>
#include <emmintrin.h>
#include <fstream> // std::ifstream
#include <immintrin.h>
#include <iostream>
#include <unistd.h>
#include <vector>
#define JSON
#ifdef JSON
#include "Symbols.hpp"
#include "buffer.hpp"
#include "comms-lib.h"
#include "gettime.h"
#include "memory_manage.h"
#include "modulation.hpp"
#include "utils.h"
#include "utils_ldpc.hpp"
#include <nlohmann/json.hpp>

using json = nlohmann::json;
#endif


class LDPCconfig {
public:
    LDPCconfig( uint16_t bg,     uint16_t zc,         int16_t max_dec_itr, 
                bool early_term, uint32_t num_cb_len, uint32_t num_cb_codew_len, 
                size_t num_rows, size_t num_blocks_in_symbol)
        : base_graph_(bg)
        , expansion_factor_(zc)
        , max_decoder_iter_(max_dec_itr)
        , early_termination_(early_term)
        , num_rows_(num_rows)
        , num_cb_len_(num_cb_len)
        , num_cb_codew_len_(num_cb_codew_len)
        , num_blocks_in_symbol_(num_blocks_in_symbol) {
    }

    // Return the number of bytes in the information bit sequence for LDPC
    // encoding of one code block
    size_t numInputBytes( void )   const { return bits_to_bytes(ldpc_num_input_bits(this->base_graph_, this->expansion_factor_)); }

    // Return the number of bytes in the encoded LDPC code word
    size_t numEncodedBytes( void ) const { return bits_to_bytes(ldpc_num_encoded_bits(this->base_graph_, this->expansion_factor_, this->num_rows_)); }

    inline void num_blocks_in_symbol ( size_t num_blocks ) { this->num_blocks_in_symbol_ = num_blocks; }

    /* Accessors */
    inline uint16_t base_graph( void ) const { return this->base_graph_; }
    inline uint16_t expansion_factor( void ) const { return this->expansion_factor_; }
    inline int16_t  max_decoder_iter( void ) const { return this->max_decoder_iter_; }
    inline bool     early_termination( void ) const { return this->early_termination_; }
    inline uint32_t num_cb_len( void ) const { return this->num_cb_len_; }
    inline uint32_t num_cb_codew_len( void ) const { return this->num_cb_codew_len_; }
    inline size_t   num_rows( void ) const { return this->num_rows_; }
    inline size_t   num_blocks_in_symbol( void ) const { return this->num_blocks_in_symbol_; }

private:
    LDPCconfig ( void ) { }

    uint16_t base_graph_; /// The 5G NR LDPC base graph (one or two)
    uint16_t expansion_factor_; /// The 5G NR LDPC expansion factor
    int16_t  max_decoder_iter_; /// Maximum number of decoder iterations per codeblock

    /// Allow the LDPC decoder to terminate without completing all iterations
    /// if it decodes the codeblock eariler
    bool     early_termination_;

    size_t   num_rows_; /// Number of rows in the LDPC base graph to use
    uint32_t num_cb_len_; /// Number of information bits input to LDPC encoding
    uint32_t num_cb_codew_len_; /// Number of codeword bits output from LDPC encodings
    size_t   num_blocks_in_symbol_;
};

class Config {
public:
    const double freq_ghz; // RDTSC frequency in GHz

    std::string modulation; // Modulation order as a string, e.g., "16QAM"
    size_t mod_order; // Modulation order (e.g., 4: QPSK, 16: 16QAM, 64: 64QAM)
    size_t mod_order_bits; // Number of binary bits used for a modulation order

    // Modulation lookup table for mapping binary bits to constellation points
    Table<complex_float> mod_table;

    std::vector<std::string> radio_ids;
    std::vector<std::string> hub_ids;

    // Controls whether the synchronization and frame time keeping is done
    // in hardware or software
    // true: use hardware correlator; false: use software corrleator
    bool hw_framer;

    std::vector<std::complex<float>> gold_cf32;
    std::vector<std::complex<int16_t>> beacon_ci16;
    std::vector<std::vector<uint32_t>> beacon_weights;
    std::vector<uint32_t> coeffs;
    std::vector<std::complex<int16_t>> pilot_ci16;
    std::vector<std::complex<float>> pilot_cf32;
    std::vector<uint32_t> pilot;
    std::vector<uint32_t> beacon;
    complex_float* pilots_;
    complex_float* pilots_sgn_;
    Table<complex_float> ue_specific_pilot;
    Table<std::complex<int16_t>> ue_specific_pilot_t;
    std::vector<std::complex<float>> common_pilot;

    double freq;
    double rate;
    double nco;
    double radioRfFreq;
    double bwFilter;
    bool single_gain_;
    double tx_gain_a;
    double rx_gain_a;
    double tx_gain_b;
    double rx_gain_b;
    double calib_tx_gain_a;
    double calib_tx_gain_b;
    std::vector<double> client_gain_adj_a;
    std::vector<double> client_gain_adj_b;

    size_t nCells;
    size_t nRadios;
    size_t nAntennas;
    size_t nChannels;
    size_t ref_ant;
    size_t beacon_ant;
    size_t beacon_len;
    bool beamsweep;
    bool sampleCalEn;
    bool imbalanceCalEn;
    bool recipCalEn;
    bool external_ref_node;
    std::string channel;
    size_t ant_group_num;
    size_t ant_per_group;

    size_t core_offset;
    size_t worker_thread_num;
    size_t socket_thread_num;
    size_t fft_thread_num;
    size_t demul_thread_num;
    size_t decode_thread_num;
    size_t zf_thread_num;

    // Number of OFDM data subcarriers handled in one demodulation event
    size_t demul_block_size;
    size_t demul_events_per_symbol; // Derived from demul_block_size

    // Number of OFDM data subcarriers handled in one doZF function call
    size_t zf_block_size;

    // Number of doZF function call handled in on event
    size_t zf_batch_size;
    size_t zf_events_per_symbol; // Derived from zf_block_size

    // Number of antennas handled in one FFT event
    size_t fft_block_size;

    // Number of code blocks handled in one encode event
    size_t encode_block_size;

    bool freq_orthogonal_pilot;

    // The number of zero IQ samples prepended to a time-domain symbol (i.e.,
    // before the cyclic prefix) before transmission. Its value depends on
    // over-the-air and RF delays, and is currently calculated by manual tuning.
    size_t ofdm_tx_zero_prefix_;

    // The number of zero IQ samples appended to a time-domain symbol before
    // transmission. Its value depends on over-the-air and RF delays, and is
    // currently calculated by manual tuning.
    size_t ofdm_tx_zero_postfix_;

    // The number of IQ samples to skip from the beginning of symbol received by
    // Agora on the uplink at the base station. Due to over-the-air and RF
    // delays, this can be different from (prefix + cp_len_), and is currently
    // calculated by manual tuning.
    size_t ofdm_rx_zero_prefix_bs_;

    size_t ofdm_rx_zero_prefix_cal_ul_;
    size_t ofdm_rx_zero_prefix_cal_dl_;

    // The number of IQ samples to skip from the beginning of symbol received by
    // Agora on the downlink at the client. Due to over-the-air and RF
    // delays, this can be different from (prefix + cp_len_), and is currently
    // calculated by manual tuning.
    size_t ofdm_rx_zero_prefix_client_;

    // The total number of IQ samples in one physical layer time-domain packet
    // received or sent by Agora
    size_t sampsPerSymbol;

    // The number of bytes in one physical layer time-domain packet received or
    // sent by Agora. This includes Agora's packet header, but not the
    // Ethernet/IP/UDP headers.
    size_t packet_length;

    int cl_tx_advance;
    // Indicates all UEs that are in this experiment,
    // including the ones instantiated on other runs/machines.
    size_t total_ue_ant_num;
    // Indicates the (pilot) offset of the UEs in this instance,
    // with respect to all UEs used in the same experiment
    size_t ue_ant_offset;
    float scale; // Scaling factor for all transmit symbols

    // Total number of symbols in a frame, including all types of symbols (e.g.,
    // pilot symbols, uplink and downlink data symbols, and calibration symbols)
    size_t symbol_num_perframe;

    // Total number of beacon symbols in a frame
    size_t beacon_symbol_num_perframe;

    // Total number of pilot symbols in a frame
    size_t pilot_symbol_num_perframe;

    // Total number of data symbols in a frame, including uplink data symbols
    // and downlink data symbols
    size_t data_symbol_num_perframe;

    // Total number of pilot symbols in a frame
    size_t recip_pilot_symbol_num_perframe;

    bool bigstation_mode; // If true, use pipeline-parallel scheduling
    bool correct_phase_shift; // If true, do phase shift correction

    // The total number of uncoded data bytes in each OFDM symbol
    size_t data_bytes_num_persymbol;

    // The total number of MAC payload data bytes in each Frame
    size_t mac_data_bytes_num_perframe;

    // The total number of MAC packet bytes in each Frame
    size_t mac_bytes_num_perframe;

    // The length (in bytes) of a MAC packet including the header
    size_t mac_packet_length;

    // The length (in bytes) of a MAC packet payload
    size_t mac_payload_length;

    // The total number of mac packets sent/received in each frame
    size_t mac_packets_perframe;

    // IP address of the machine running the baseband processing for UE
    std::string ue_server_addr;

    // IP address of the machine running the baseband processing for BS
    std::string bs_server_addr;

    // IP address of the base station RRU, RRU emulator (sender),
    // or channel simulator
    std::string bs_rru_addr;

    // IP address of the data source/sink server communicating with MAC (BS/UE)
    std::string mac_remote_addr;

    int bs_server_port; // Base UDP port used by BS to receive data

    // Base RRU/channel simulator UDP port used by BS to transmit downlink data
    int bs_rru_port;

    int ue_server_port; // Base UDP port used by UEs to receive data

    // Base RRU/channel simulator UDP port used by UEs to transmit uplink data
    int ue_rru_port;

    // Number of NIC ports used for DPDK
    uint16_t dpdk_num_ports;

    // Port ID at MAC layer side
    int mac_rx_port;
    int mac_tx_port;
    bool init_mac_running;

    // Number of frames_ sent by sender during testing = number of frames_
    // processed by Agora before exiting.
    size_t frames_to_test;

    // Size of tranport block given by upper layer
    size_t transport_block_size;

    float noise_level;

    // Number of bytes per code block
    size_t num_bytes_per_cb;

    bool fft_in_rru; // If true, the RRU does FFT instead of Agora

    bool isUE;
    const size_t maxFrame = 1 << 30;
    const size_t data_offset = sizeof(int) * 16;

    size_t getNumAntennas( void ) { return (nRadios * nChannels); }
    
    int getSymbolId(size_t symbol_id);

    // Get the index of this downlink symbol among this frame's downlink symbols
    size_t get_dl_symbol_idx(size_t frame_id, size_t symbol_id) const;

    // Get the index of this uplink symbol among this frame's uplink symbols
    size_t get_ul_symbol_idx(size_t frame_id, size_t symbol_id) const;

    // Get the index of this pilot symbol among this frame's pilot symbols
    size_t get_pilot_symbol_idx(size_t frame_id, size_t symbol_id) const;

    bool isPilot(size_t, size_t);
    bool isCalDlPilot(size_t, size_t);
    bool isCalUlPilot(size_t, size_t);
    bool isDownlink(size_t, size_t);
    bool isUplink(size_t, size_t);

    /// Return the symbol type of this symbol in this frame
    SymbolType get_symbol_type(size_t frame_id, size_t symbol_id);

    /// Return the single-gain control decision
    inline bool single_gain( void ) const { return this->single_gain_; }

    inline void update_mod_cfgs(size_t new_mod_order_bits)
    {
        mod_order_bits = new_mod_order_bits;
        mod_order = static_cast<size_t>(pow(2, mod_order_bits));
        init_modulation_table(mod_table, mod_order);
        this->ldpc_config_.num_blocks_in_symbol( 
            (ofdm_data_num_ * mod_order_bits) / this->ldpc_config_.num_cb_codew_len() );
    }

    /// Return total number of data symbols of all frames_ in a buffer
    /// that holds data of kFrameWnd frames_
    inline size_t get_total_data_symbol_idx(
        size_t frame_id, size_t symbol_id) const
    {
        return ((frame_id % kFrameWnd) * data_symbol_num_perframe) + symbol_id;
    }

    /// Return total number of uplink data symbols of all frames_ in a buffer
    /// that holds data of kFrameWnd frames_
    inline size_t get_total_data_symbol_idx_ul(
        size_t frame_id, size_t symbol_idx_ul) const
    {
        return ((frame_id % kFrameWnd) * ul_data_symbol_num_perframe_)
            + symbol_idx_ul;
    }

    /// Return total number of downlink data symbols of all frames_ in a buffer
    /// that holds data of kFrameWnd frames_
    inline size_t get_total_data_symbol_idx_dl(
        size_t frame_id, size_t symbol_idx_dl) const
    {
        return ((frame_id % kFrameWnd) * dl_data_symbol_num_perframe_)
            + symbol_idx_dl;
    }

    /// Return the frame duration in seconds
    inline double get_frame_duration_sec( void )
    {
        return ((symbol_num_perframe * sampsPerSymbol) / rate);
    }

    /// Fetch the data buffer for this frame and symbol ID. The symbol must
    /// be an uplink symbol.
    inline complex_float* get_data_buf(Table<complex_float>& data_buffers,
        size_t frame_id, size_t symbol_id) const
    {
        size_t frame_slot = frame_id % kFrameWnd;
        size_t symbol_offset = (frame_slot * ul_data_symbol_num_perframe_)
            + get_ul_symbol_idx(frame_id, symbol_id);
        return data_buffers[symbol_offset];
    }

    /// Return the subcarrier ID to which we should refer to for the zeroforcing
    /// matrices of subcarrier [sc_id].
    inline size_t get_zf_sc_id(size_t sc_id) const
    {
        return freq_orthogonal_pilot ? sc_id - (sc_id % ue_num_) : sc_id;
    }

    /// Get the calibration buffer for this frame and subcarrier ID
    inline complex_float* get_calib_buffer(
        Table<complex_float>& calib_buffer, size_t frame_id, size_t sc_id) const
    {
        size_t frame_slot = frame_id % kFrameWnd;
        return &calib_buffer[frame_slot][sc_id * bs_ant_num_];
    }

    /// Get the decode buffer for this frame, symbol, user and code block ID
    inline uint8_t* get_decode_buf(Table<uint8_t>& decoded_buffer,
        size_t frame_id, size_t symbol_id, size_t ue_id, size_t cb_id) const
    {
        size_t total_data_symbol_id
            = get_total_data_symbol_idx_ul(frame_id, symbol_id);
        return &decoded_buffer[total_data_symbol_id][roundup<64>(
                                                         num_bytes_per_cb)
            * (ldpc_config_.num_blocks_in_symbol() * ue_id + cb_id)];
    }

    /// Get ul_bits for this symbol, user and code block ID
    inline int8_t* get_info_bits(Table<int8_t>& info_bits, size_t symbol_id,
        size_t ue_id, size_t cb_id) const
    {
        return &info_bits[symbol_id][roundup<64>(num_bytes_per_cb)
            * (ldpc_config_.num_blocks_in_symbol() * ue_id + cb_id)];
    }

    /// Get encoded_buffer for this frame, symbol, user and code block ID
    inline int8_t* get_encoded_buf(Table<int8_t>& encoded_buffer,
        size_t frame_id, size_t symbol_id, size_t ue_id, size_t cb_id) const
    {
        size_t total_data_symbol_id
            = get_total_data_symbol_idx_dl(frame_id, symbol_id);
        size_t num_encoded_bytes_per_cb
            = ldpc_config_.num_cb_codew_len() / mod_order_bits;
        return &encoded_buffer[total_data_symbol_id]
                              [roundup<64>(ofdm_data_num_) * ue_id
                                  + num_encoded_bytes_per_cb * cb_id];
    }

    // Returns the number of pilot subcarriers in downlink symbols used for
    // phase tracking
    inline size_t get_ofdm_pilot_num( void ) const
    {
        return ofdm_data_num_ / ofdm_pilot_spacing_;
    }

    Config(std::string);
    void genData(void);
    ~Config(void);

    inline size_t bs_ant_num( void )         const { return this->bs_ant_num_; }
    inline void   bs_ant_num( size_t n_bs_ant ) { this->bs_ant_num_ = n_bs_ant; }

    inline size_t bf_ant_num( void )         const { return this->bf_ant_num_; } 
    inline size_t ue_num( void )             const { return this->ue_num_; } 
    inline size_t ue_ant_num( void )         const { return this->ue_ant_num_; } 
    inline size_t ofdm_ca_num( void )        const { return this->ofdm_ca_num_; } 
    inline size_t cp_len( void )             const { return this->cp_len_; } 
    inline size_t ofdm_data_num( void )      const { return this->ofdm_data_num_; } 
    inline size_t ofdm_data_start( void )    const { return this->ofdm_data_start_; } 
    inline size_t ofdm_data_stop( void )     const { return this->ofdm_data_stop_; } 
    inline size_t ofdm_pilot_spacing( void ) const { return this->ofdm_pilot_spacing_; } 

    inline bool   downlink_mode( void )           const { return this->downlink_mode_; }
    inline const  LDPCconfig& ldpc_config( void ) const { return this->ldpc_config_; }

    inline const std::vector<std::string>& frames( void ) const { return this->frames_; }

    inline const std::vector<std::vector<size_t>>& pilot_symbols  ( void ) const { return this->pilot_symbols_; }
    inline const std::vector<std::vector<size_t>>& ul_symbols     ( void ) const { return this->ul_symbols_; }
    inline const std::vector<std::vector<size_t>>& dl_symbols     ( void ) const { return this->dl_symbols_; }
    inline const std::vector<std::vector<size_t>>& dl_cal_symbols ( void ) const { return this->dl_cal_symbols_; }

    inline void   running( bool value ) { this->running_.store(value); }
    inline bool   running( void ) const { return this->running_.load(); }

    inline size_t ul_data_symbol_num_perframe( void ) const { return this->ul_data_symbol_num_perframe_; }
    inline size_t ul_pilot_syms( void )               const { return this->ul_pilot_syms_; }
    inline size_t dl_data_symbol_num_perframe( void ) const { return this->dl_data_symbol_num_perframe_; }
    inline size_t dl_data_symbol_start( void )        const { return this->dl_data_symbol_start_; }
    inline size_t dl_data_symbol_end( void )          const { return this->dl_data_symbol_end_; }
    inline size_t dl_pilot_syms( void )               const { return this->dl_pilot_syms_; }
    inline size_t dl_packet_length( void )            const { return this->dl_packet_length_; }


    inline Table<int8_t>& dl_bits( void ) { return this->dl_bits_; }
    inline Table<int8_t>& ul_bits( void ) { return this->ul_bits_; }
    inline Table<complex_float>& ul_iq_f ( void ) { return this->ul_iq_f_; }
    inline Table<std::complex<int16_t>>& dl_iq_t ( void ) { return this->dl_iq_t_; }

private:
    size_t bs_ant_num_; // Total number of BS antennas
    size_t bf_ant_num_; // Number of antennas used in beamforming
    size_t ue_num_;
    size_t ue_ant_num_;

    // The total number of OFDM subcarriers, which is a power of two
    size_t ofdm_ca_num_;

    // The number of cyclic prefix IQ samples. These are taken from the tail of
    // the time-domain OFDM samples and prepended to the beginning.
    size_t cp_len_;

    // The number of OFDM subcarriers that are non-zero in the frequency domain
    size_t ofdm_data_num_;

    // The index of the first non-zero OFDM subcarrier (in the frequency domain)
    // in block of ofdm_ca_num_ subcarriers.
    size_t ofdm_data_start_;

    // The index of the last non-zero OFDM subcarrier (in the frequency domain)
    // in block of ofdm_ca_num_ subcarriers.
    size_t ofdm_data_stop_;

    size_t ofdm_pilot_spacing_;

    bool downlink_mode_; // If true, the frame contains downlink symbols

    LDPCconfig ldpc_config_; // LDPC parameters

    // A string in \p frames_ contains letters representing the symbol types in
    // the frame (e.g., 'P' for pilot symbols, 'U' for uplink data symbols)
    std::vector<std::string> frames_;

    // beacon_symbols_[i] contains IDs of beacon symbols in frames_[i]
    std::vector<std::vector<size_t>> beacon_symbols_; /* No accessor */

    // pilot_symbols_[i] contains IDs of pilot symbols in frames_[i]
    std::vector<std::vector<size_t>> pilot_symbols_;

    // ul_symbols_[i] contains IDs of uplink data symbols in frames_[i]
    std::vector<std::vector<size_t>> ul_symbols_;

    // dl_symbols_[i] contains IDs of downlink data symbols in frames_[i]
    std::vector<std::vector<size_t>> dl_symbols_;

    // ul_cal_symbols_[i] contains IDs of uplink calibration symbols in
    // frames_[i]
    std::vector<std::vector<size_t>> ul_cal_symbols_; /* No accessor */

    // dl_cal_symbols_[i] contains IDs of downlink calibration symbols in
    // frames_[i]
    std::vector<std::vector<size_t>> dl_cal_symbols_;

    std::atomic<bool> running_;

    /* Downlink / uplink tracking variables */
    size_t ul_data_symbol_num_perframe_;
    size_t ul_pilot_syms_;

    size_t dl_data_symbol_num_perframe_;
    size_t dl_data_symbol_start_;
    size_t dl_data_symbol_end_;

    size_t dl_pilot_syms_;
    size_t dl_packet_length_; // HAS_TIME & END_BURST, fixme


    Table<int8_t> dl_bits_;
    Table<int8_t> ul_bits_;
    Table<int8_t> ul_encoded_bits;
    Table<uint8_t> ul_mod_input;
    Table<uint8_t> dl_mod_input;
    Table<complex_float> dl_iq_f_;
    Table<complex_float> ul_iq_f_;
    Table<std::complex<int16_t>> dl_iq_t_;
    Table<std::complex<int16_t>> ul_iq_t_;
};
#endif
