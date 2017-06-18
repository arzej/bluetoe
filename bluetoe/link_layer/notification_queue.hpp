#ifndef BLUETOE_LINK_LAYER_NOTIFICATION_QUEUE_HPP
#define BLUETOE_LINK_LAYER_NOTIFICATION_QUEUE_HPP

#include <cstdint>
#include <cstdlib>
#include <utility>
#include <cassert>
#include <algorithm>
#include <iterator>

namespace bluetoe {
namespace link_layer {

    namespace details {
        /**
         * @brief type of entry
         */
        enum notification_queue_entry_type {
            /** returned if there no entry */
            empty,
            /** returned if the entry is a notification */
            notification,
            /** returned if the entry is an indication */
            indication
        };

        template < typename Size >
        class notification_queue_impl_base;
    }

    /**
     * @brief class responsible to keep track of those characteristics that have outstanding
     *        notifications or indications.
     *
     * All operations on the queue must be reentrent / atomic!
     *
     * @param Sizes List of number of characteristics that have notifications and / or indications enabled by priorities.
     * @param Mixin a class to be mixed in, to allow empty base class optimizations
     *
     * For all function, index is an index into a list of all the characterstics with notifications / indications
     * enable. The queue is implemented by an array that contains a few bits (2) per characteristic to store the
     * requested (or queued) notifications / indications.
     */
    template < typename Sizes, class Mixin >
    class notification_queue : public Mixin, details::notification_queue_impl_base< Sizes >
    {
    public:
        using entry_type = details::notification_queue_entry_type;

        /**
         * @brief constructs an empty notification_queue_impl
         *
         * All constructor arguments are ment to be passed to the derived Mixin.
         */
        template < class ... Args >
        notification_queue( Args... mixin_arguments );

        /**
         * @brief queue the indexed characteristic for notification
         *
         * Once a characteristic is queued for notification, the function
         * dequeue_indication_or_confirmation() will return { notification, index }
         * on a future call.
         *
         * If the given characteristic was already queued for notification, the function
         * will not have any side effects.
         *
         * The function returns true, if the given characteristic was not already queued for notifications.
         *
         * @pre index < Size
         */
        bool queue_notification( std::size_t index );

        /**
         * @brief queue the indexed characteristic for indication
         *
         * Once a characteristic is queued for indication, the function
         * dequeue_indication_or_confirmation() will return { indication, index }
         * om a future call.
         *
         * If the given characteristic was already queued for indication, or
         * if a indication that was send to a client was not confirmed yet,
         * the function will not have any side effects.
         *
         * The function returns true, if the given characteristic was not already queued for indication or
         * if a confirmations is still awaited.
         *
         * @pre index < Size
         */
        bool queue_indication( std::size_t index );

        /**
         * @brief to be called, when a ATT Handle Value Confirmation was received.
         *
         * If not outstanding confirmation is registered, the
         * function has not side effect.
         */
        void indication_confirmed();

        /**
         * @brief return a next notification or indication to be send.
         *
         * For a returned notification, the function will remove the returned entry.
         * For a returned indication, the function will change the entry to 'unconfirmed' and
         * will not return any indications until indication_confirmed() is called for the
         * returned index.
         */
        std::pair< details::notification_queue_entry_type, std::size_t > dequeue_indication_or_confirmation();

        /**
         * @brief removes all entries from the queue
         */
        void clear_indications_and_confirmations();

    private:
        using impl = details::notification_queue_impl_base< Sizes >;
        std::size_t outstanding_confirmation_index_;
    };

    // impl
    template < typename Sizes, class Mixin >
    template < class ... Args >
    notification_queue< Sizes, Mixin >::notification_queue( Args... mixin_arguments )
        : Mixin( mixin_arguments... )
        , outstanding_confirmation_index_( ~0 )
    {
    }

    template < typename Sizes, class Mixin >
    bool notification_queue< Sizes, Mixin >::queue_notification( std::size_t index )
    {
        return impl::queue_notification( index );
    }

    template < typename Sizes, class Mixin >
    bool notification_queue< Sizes, Mixin >::queue_indication( std::size_t index )
    {
        return impl::queue_indication( index );
    }

    template < typename Sizes, class Mixin >
    void notification_queue< Sizes, Mixin >::indication_confirmed()
    {
        impl::indication_confirmed();
    }

    template < typename Sizes, class Mixin >
    std::pair< details::notification_queue_entry_type, std::size_t > notification_queue< Sizes, Mixin >::dequeue_indication_or_confirmation()
    {
        return impl::dequeue_indication_or_confirmation();
    }

    template < typename Sizes, class Mixin >
    void notification_queue< Sizes, Mixin >::clear_indications_and_confirmations()
    {
        outstanding_confirmation_index_ = ~0;
        impl::clear_indications_and_confirmations();
    }

    namespace details
    {
        template < int Size >
        class notification_queue_impl
        {
        public:
            notification_queue_impl()
                : outstanding_confirmation_( Size )
            {
                clear_indications_and_confirmations();
            }

            bool queue_notification( std::size_t index )
            {
                assert( index < Size );
                return add( index, notification_bit );
            }

            bool queue_indication( std::size_t index )
            {
                assert( index < Size );

                if ( outstanding_confirmation_ == index )
                    return false;

                return add( index, indication_bit );
            }

            void indication_confirmed()
            {
                if ( outstanding_confirmation_ != Size )
                {
                    outstanding_confirmation_ = Size;
                }
            }

            std::pair< notification_queue_entry_type, std::size_t > dequeue_indication_or_confirmation()
            {
                bool ignore_first = true;

                // loop over all entries in a circle
                for ( std::size_t i = next_; ignore_first || i != next_; i = ( i + 1 ) % Size )
                {
                    ignore_first = false;
                    auto entry = at( i );

                    if ( entry & indication_bit && outstanding_confirmation_ == Size )
                    {
                        outstanding_confirmation_ = i;
                        next_ = ( i + 1 ) % Size;
                        remove( i, indication_bit );
                        return { indication, i };
                    }
                    else if ( entry & notification_bit )
                    {
                        next_ = ( i + 1 ) % Size;
                        remove( i, notification_bit );
                        return { notification, i };
                    }

                }

                return { empty, 0 };
            }

            void clear_indications_and_confirmations()
            {
                next_ = 0;
                outstanding_confirmation_ = Size;
                std::fill( std::begin( queue_ ), std::end( queue_ ), 0 );
            }

        private:
            int at( std::size_t index )
            {
                const auto bit_offset  = ( index * bits_per_characteristc ) % 8;
                const auto byte_offset = index * bits_per_characteristc / 8;
                assert( byte_offset < sizeof( queue_ ) / sizeof( queue_[ 0 ] ) );

                return ( queue_[ byte_offset ] >> bit_offset ) & 0x03;
            }

            bool add( std::size_t index, int bits )
            {
                assert( bits & ( ( 1 << bits_per_characteristc ) -1 ) );
                const auto bit_offset  = ( index * bits_per_characteristc ) % 8;
                const auto byte_offset = index * bits_per_characteristc / 8;
                assert( byte_offset < sizeof( queue_ ) / sizeof( queue_[ 0 ] ) );

                const bool result = ( queue_[ byte_offset ] & ( bits << bit_offset ) ) == 0;
                queue_[ byte_offset ] |= bits << bit_offset;

                return result;
            }

            void remove( std::size_t index, int bits )
            {
                assert( bits & ( ( 1 << bits_per_characteristc ) -1 ) );
                const auto bit_offset  = ( index * bits_per_characteristc ) % 8;
                const auto byte_offset = index * bits_per_characteristc / 8;
                assert( byte_offset < sizeof( queue_ ) / sizeof( queue_[ 0 ] ) );

                queue_[ byte_offset ] &= ~( bits << bit_offset );
            }

            static constexpr std::size_t bits_per_characteristc = 2;

            enum char_bits {
                notification_bit = 0x01,
                indication_bit   = 0x02
            };

            std::size_t     next_;
            std::uint8_t    queue_[ ( Size * bits_per_characteristc + 7 ) / 8 ];

            std::size_t     outstanding_confirmation_;
        };

        /**
         * @brief Specialisation for one characteritics with notification or indication enabled
         */
        template <>
        class notification_queue_impl< 1 >
        {
        public:
            notification_queue_impl()
                : state_( empty )
            {
            }

            bool queue_notification( std::size_t idx )
            {
                assert( idx == 0 );

                const bool result = state_ == empty;

                if ( result )
                    state_ = notification;

                return result;
            }

            bool queue_indication( std::size_t idx)
            {
                assert( idx == 0 );

                const bool result = state_ == empty;

                if ( result )
                    state_ = indication;

                return result;
            }

            void indication_confirmed()
            {
                if ( state_ == outstanding_confirmation )
                    state_ = empty;
            }

            std::pair< notification_queue_entry_type, std::size_t > dequeue_indication_or_confirmation()
            {
                const auto result = state_ == outstanding_confirmation
                    ? std::pair< notification_queue_entry_type, std::size_t >{ empty, 0 }
                    : std::pair< notification_queue_entry_type, std::size_t >{ static_cast< notification_queue_entry_type >( state_ ), 0 };

                state_ = state_ == indication || state_ == outstanding_confirmation
                    ? outstanding_confirmation
                    : empty;

                return result;
            }

            void clear_indications_and_confirmations()
            {
                state_ = empty;
            }
        private:
            enum {
                outstanding_confirmation = indication + 1
            };

            std::uint8_t state_;
        };

        template <>
        class notification_queue_impl_base< std::tuple<> >
        {
        public:
            bool queue_notification( std::size_t ) { return false; }
            bool queue_indication( std::size_t ) { return false; }
            void indication_confirmed() {}

            std::pair< notification_queue_entry_type, std::size_t > dequeue_indication_or_confirmation()
            {
                return { notification_queue_entry_type::empty, 0 };
            }

            void clear_indications_and_confirmations() {}
        };

        template < int Size, class ...Ts >
        class notification_queue_impl_base< std::tuple< std::integral_constant< int, Size >, Ts... > >
            : public notification_queue_impl_base< std::tuple< Ts... > >
            , private notification_queue_impl< Size >
        {
        public:
            using base = notification_queue_impl_base< std::tuple< Ts... > >;
            using impl = notification_queue_impl< Size >;

            bool queue_notification( std::size_t idx )
            {
                return idx < Size
                    ? impl::queue_notification( idx )
                    : base::queue_notification( idx - Size );
            }

            bool queue_indication( std::size_t idx )
            {
                return idx < Size
                    ? impl::queue_indication( idx )
                    : base::queue_indication( idx - Size );
            }

            void indication_confirmed()
            {
                impl::indication_confirmed();
                base::indication_confirmed();
            }

            std::pair< notification_queue_entry_type, std::size_t > dequeue_indication_or_confirmation()
            {
                auto result = impl::dequeue_indication_or_confirmation();

                if ( result.first != notification_queue_entry_type::empty )
                    return result;

                result = base::dequeue_indication_or_confirmation();

                if ( result.first != notification_queue_entry_type::empty )
                    result.second += Size;

                return result;
            }

            void clear_indications_and_confirmations()
            {
                impl::clear_indications_and_confirmations();
                base::clear_indications_and_confirmations();
            }
        };

    } // namespace details

} // namespace link_layer
} // namespace bluetoe

#endif // include guard
