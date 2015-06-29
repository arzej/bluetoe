#ifndef BLUETOE_TESTS_LINK_LAYER_TEST_RADIO_HPP
#define BLUETOE_TESTS_LINK_LAYER_TEST_RADIO_HPP

#include <bluetoe/link_layer/buffer.hpp>
#include <bluetoe/link_layer/delta_time.hpp>
#include <vector>
#include <functional>

namespace test {

    /**
     * @brief stores all relevant arguments to a schedule function call to the radio
     */
    struct schedule_data
    {
        bluetoe::link_layer::delta_time schedule_time;     // when was the actions scheduled (from start of simulation)
        bluetoe::link_layer::delta_time on_air_time;       // when was the action on air (from start of simulation)

        // parameters
        unsigned                        channel;
        bluetoe::link_layer::delta_time transmision_time;
        std::vector< std::uint8_t >     transmitted_data;
        bluetoe::link_layer::delta_time timeout;
    };

    class radio_base
    {
    public:
        // test interface
        const std::vector< schedule_data >& scheduling() const;

        /**
         * @brief calls check with every scheduled_data
         */
        void check_scheduling( const std::function< bool ( const schedule_data& ) >& check, const char* message ) const;

        /**
         * @brief calls check with adjanced pairs of schedule_data.
         */
        void check_scheduling( const std::function< bool ( const schedule_data& first, const schedule_data& next ) >& check, const char* message ) const;
        void check_scheduling( const std::function< bool ( const schedule_data& ) >& filter, const std::function< bool ( const schedule_data& first, const schedule_data& next ) >& check, const char* message ) const;

        void all_data( std::function< void ( const schedule_data& ) > ) const;
    protected:
        std::vector< schedule_data >::const_iterator next( std::vector< schedule_data >::const_iterator, const std::function< bool ( const schedule_data& ) >& filter ) const;

        std::vector< schedule_data > transmitted_data_;
    };

    template < typename CallBack >
    class radio : public radio_base
    {
    public:
        /**
         * @brief by default the radio simulates 10s without any response
         */
        radio();

        // scheduled_radio interface
        void schedule_transmit_and_receive(
                unsigned channel,
                const bluetoe::link_layer::write_buffer& transmit, bluetoe::link_layer::delta_time when,
                const bluetoe::link_layer::read_buffer& receive, bluetoe::link_layer::delta_time timeout );

        void run();
    private:
        // end of simulations
        const bluetoe::link_layer::delta_time eos_;
              bluetoe::link_layer::delta_time now_;
    };

    // implementation
    template < typename CallBack >
    radio< CallBack >::radio()
        : eos_( bluetoe::link_layer::delta_time::seconds( 10 ) )
        , now_( bluetoe::link_layer::delta_time::now() )
    {
    }

    template < typename CallBack >
    void radio< CallBack >::schedule_transmit_and_receive(
            unsigned channel,
            const bluetoe::link_layer::write_buffer& transmit, bluetoe::link_layer::delta_time when,
            const bluetoe::link_layer::read_buffer& receive, bluetoe::link_layer::delta_time timeout )
    {
        const schedule_data data{
            now_,
            now_ + when,
            channel,
            when,
            std::vector< std::uint8_t >( transmit.buffer, transmit.buffer + transmit.size ),
            timeout
        };

        transmitted_data_.push_back( data );
    }

    template < typename CallBack >
    void radio< CallBack >::run()
    {
        assert( !transmitted_data_.empty() );

        do
        {
            // for now, only timeouts are simulated
            if ( true )
            {
                now_ += transmitted_data_.back().timeout;
                static_cast< CallBack* >( this )->timeout();
            }
        } while ( now_ < eos_ );
    }
}

#endif // include guard
