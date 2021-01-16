// Copyright (c) 2018-2020, Rice University
// RENEW OPEN SOURCE LICENSE: http://renew-wireless.org/license

/**
 * @file framestats.h
 * @brief Class defination for frame tracking
 * @author Rice University
 */

#ifndef FRAMESTATS_H_
#define FRAMESTATS_H_

#include <string>
#include <vector>

class FrameStats
{
    public:
        FrameStats(std::string new_frame_id);
        FrameStats(std::string new_frame_id, size_t ul, size_t dl);

        void SetClientPilotSyms( size_t ul, size_t dl );

        size_t NumDLCalSyms( void ) const;
        size_t NumULCalSyms( void ) const;
        size_t NumDLSyms( void ) const;
        size_t NumULSyms( void ) const;
        size_t NumPilotSyms( void ) const;
        size_t NumBeaconSyms( void ) const;
        size_t NumTotalSyms( void ) const;
        
        size_t GetDLSymbol( size_t location ) const;
        inline size_t GetDLSymbolLast( void ) const { return ((this->dl_symbols_.size() == 0) ? SIZE_MAX : this->dl_symbols_.back()); }
        /* Returns SIZE_MAX if there are no DL symbols */
        size_t GetDLSymbolIdx ( size_t symbol_number ) const;

        size_t GetULSymbol( size_t location ) const;
        inline size_t GetULSymbolLast( void ) const { return ((this->ul_symbols_.size() == 0) ? SIZE_MAX : this->ul_symbols_.back()); }
        /* Returns SIZE_MAX if there are no UL symbols */
        size_t GetULSymbolIdx ( size_t symbol_number ) const;

        size_t GetPilotSymbol( size_t location ) const;
        size_t GetPilotSymbolIdx ( size_t symbol_number ) const;

        size_t GetDLCalSymbol( size_t location ) const;
        size_t GetULCalSymbol( size_t location ) const;

        bool   IsRecCalEnabled( void ) const;
        size_t NumDataSyms( void ) const;

        /* Accessors */
        inline const std::string &frame_identifier( void )  const { return frame_identifier_; }
        inline size_t client_ul_pilot_symbols( void ) const { return client_ul_pilot_symbols_; }
        inline size_t client_dl_pilot_symbols( void ) const { return client_dl_pilot_symbols_; }

    private:
        std::string         frame_identifier_;

        std::vector<size_t> beacon_symbols_;
        std::vector<size_t> pilot_symbols_;
        std::vector<size_t> ul_symbols_;
        std::vector<size_t> ul_cal_symbols_;
        std::vector<size_t> dl_symbols_;
        std::vector<size_t> dl_cal_symbols_;

        size_t client_ul_pilot_symbols_;
        size_t client_dl_pilot_symbols_;

        /* Helper function */
        static size_t GetSymbolIdx ( const std::vector<size_t>& search_vector, size_t symbol_number );
}; /* class FrameStats */

#endif /* FRAMESTATS_H_ */