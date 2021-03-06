/*! @mainpage Bluetoe

@section intro_sec Introduction

Bluetoe is an attempt to simplify the implementation of firmware for Bluetooth Low Energy devices. Bluetooth Low Energy devices / peripherals implement a so called GATT Server. GATT is the abreviation of Generic Attribute Profile. GATT is a protocol that allows a computer (desktop, phone etc.) to discover remote devices capabilities and to interact with this devices in an unified manner.

A lot of possible device capabilities are specified by the <a href="https://www.bluetooth.org">Bluetooth Special Interest Group</a>. Others capabilities are user defined and make sense only to the implementer of a device and requires a special client that knows how to use this capabilities. Those capabilities and the means how to access them are called profiles.

@section gatt_basics GATT Basics

@subsection Characteristics

The basic building blocks of GATT are characteristics. A \link bluetoe::characteristic characteristic \endlink can be thinked of as a piece of information / a variable that resides inside of a device, which clients can interact with (discover, reading, writing). To identify a characteristic, an identifier, called a UUID is used. Beside some very basic properties like "readable" or "writeable", a characteristic can have additional properties like a name or structure informations (e.g. this is a structure containing 1 float followed by 2 integers).

@subsection Services

A \link bluetoe::service service \endlink groups characteristics to meaningful units. A helicopter position service would for example group the X, Y, and Z position of the helicopter to a Position Service. A device can announce the implementation / existance of a service by that device, so that computers looking for a specific device can see that the device is implementing the service, without the need to connect to the device.

@subsection Profiles

A profile groups @ref Services to higher level functionality and is usually "just" a document that describes what a device must implement to be conforming to a certain profile and how client should interact with services.

@subsection UUIDs

Both, @ref Characteristics and @ref Services are identified by Universally Unique Identifiers (<a href="https://en.wikipedia.org/wiki/Universally_unique_identifier">UUID</a>). Bluetooth Low Energy basically uses two different kinds of UUIDs, one 16 bit long, the other 128 bit long.

16-bit UUIDs are exclusivly assigned by the <a href="https://www.bluetooth.org">Bluetooth Special Interest Group</a>. 128-bit UUIDs can be generated by anyone and used in custom applications that are not standardised.

16-bit UUIDs are usually notated as a 4 digit hexadecimal number. 128-bit UUIDs are grouped in packs of different size. Example: 7A5F69F4-3915-41C7-92BD-1477B35B883D.

@subsection ATT

GATT is implemented on top of ATT (Attribute Protocol). GATT basecally defines how characteristics and services are mapped to attributes and how GATT procedures are mapped to ATT procedures to access the characteristics and services. A GATT server defines all its services and characterstics with one, single table of attributes.

An attribute is a tuple, containing a handle, a type, and a value. Where a handle is a 16 bit integer, which is some kind of unique key into an attribute table. The type denoted by a UUID and is either 16 bit or 128 bit long. UUIDs within the attribute table are not unique. Finally, data is a variable length data field, that can be read and / or written.

ATT not only defines this attribute table, but also how to access this table. While GATT procedures are the means of how to access the characteristics of a service, what is actually spoken on air (and thus observable or debuggable) is ATT. But this mapping of GATT procedures to ATT procedures is quit leightweigt and easy to understand and intuitive.

@section start_bluetoe Bluetoe's Implementation of GATT

Bluetoe let you define a list of GATT services on your own, let you define the characteristics within each service and how accesses to a characteristic are mapped to C++ function calls (or global variables accesses, or constants beeing reads).

@subsection Characteristic

Bluetoe uses the C++ type system to collect all data / information that is already available at compile time from the developer. So, the definition of a characteristic is a type; A template class called @ref bluetoe::characteristic which takes a list of parameters that defines how exactly a characteristic should work. Template classes that take a variable list of parameters are often used within Bluetoe and in most cases, the order of the parameters are not important. Here is a minimalistic example:

@code{.cpp}
using io_pin_access_characteristic =
    bluetoe::characteristic<
        bluetoe::characteristic_uuid< 0x43809849, 0x0025, 0x4529, 0xA50F, 0x48C362742282 >,
        bluetoe::free_write_handler< bool, io_pin_write_handler >
    >;

@endcode

The type definition above, defines a characteristic which is identifed by the 128 bit UUID 43809849-0025-4529-A50F-48C362742282 and defines, which free function have to be called, in case that there is any write attempt to the characteristic.

By omitting a definition of how to read the characteristic, bluetoe assumes that the characteristic is write-only. Any attempt to read the characteristic will be responded with an error.

Furthermore, by defing bool, to be the type, that is taken by the write handler (io_pin_write_handler()), Bluetoe will generate error responses to every attempt to write othere values that are not a single byte containing the value 1 or 0.

So the signature of io_pin_write_handler() looks like this:

@code{.cpp}
std::uint8_t io_pin_write_handler( bool state );
@endcode

The return values allows the handler to return errors, in case the requested write caused any error.

And that's all, Bluetoe needs to know to implement the access of the write-only characteristic that accepts only boolean values. Bluetoe will use this information to generate the necessary attributes in the ATT attribute table, to handle the very basic error cases and to map the characteristic access to a specific handler. No need to define handles, no need to define characteristic attributes, nor characteristic descriptors.

This shows some of the key design decisions, made for Bluetoe:
- No need to provide redundant information: No need to specify characteristic attributes, which is done implicit by the defined read and write handlers. No need to assigned handles or define descriptors, which is done by the library.
- Safe and resonable defaults are used: By narrowing down, the accepted range of values, by defining the type of stored information, Bluetoe can handle already a lot of malicious write attempts that do not fit to the underlying data type.
- Find as much bugs as possible at compile time: For example: defining a characteristic, that has neither a read handler, nor a write handler defined, will result in a compile time error.
- Make easy things easy: There should not be any need to read the bluetooth core specification before being able to read and understand the libraries documentation and there concepts.

Here is a second example of a characteristic definition:

@code{.cpp}
using temperature_characteristic =
    bluetoe::characteristic<
        bluetoe::characteristic_uuid< 0x8C8B4094, 0x0DE2, 0x499F, 0xA28A, 0x4EED5BC73CA9 >,
        bluetoe::bind_characteristic_value< decltype( temperature ), &temperature >,
        bluetoe::no_write_access
    >;
@endcode

This time, accessing the characteristic, is mapped to a global variable named temperature, by using the @ref bluetoe::bind_characteristic_value template, that takes a variable type and the address of a variable.

This would make Bluetoe to generate code that allows to read and write the characteristic value, as long as the size of the data written has the size of the given type. By adding the type @ref bluetoe::no_write_access parameter, Bluetoe will remove the write access to the characteristic and will respond with an error code to every attempt to write to the characteristic.

There are a lot more types that defines the binding to characteristic values.

@subsection Service

Roughly saying, a @ref bluetoe::service is just an UUID with a list of characteristics! So this example should make sense to you:

@code{.cpp}
using temperature_and_io_pin_service =
    bluetoe::service<
        bluetoe::service_uuid< 0xC11169E1, 0x6252, 0x4450, 0x931C, 0x1B43A318783B >,
        io_pin_access_characteristic,
        temperature_characteristic
    >;
@endcode

This service somehow combines the reading of a temperatur and the setting of an IO pin. Maybe the IO pin is connected to an actor, that can open or close a window and thus influence the temperature.

@subsection Server

Finally a @ref bluetoe::server combines all @ref bluetoe::service to implement a GATT server:

@code{.cpp}
using gatt_server =
    bluetoe::server<
       temperature_and_io_pin_service
    >;
@endcode

This is a very minimalistic example. A more sophisticated example would take serveral services and would give the server a name and such details. Bluetoe already applies a lot of defaults. A @ref bluetoe::gap_service_for_gatt_servers is added by default to the server.

@section hardware_binding Binding to Hardware

Up to now, there is no actual hardware involved. To deploy a defined GATT server to specific hardware, a so called binding is used, that takes the @ref bluetoe::server instance as parameter. Here an example that uses a nrf52 from Nordic as target hardware:

@code{.cpp}

bluetoe::nrf52< gatt_server > server;

int main()
{
    for ( ;; )
        server.run();
}
@endcode

bluetoe::nrf52<> is just a template alias that points to bluetoe::link_layer::link_layer, which takes additonal link layer configuration arguments. Again, Bluetoe applies resonable default to the link layer configuration. But if needed, parameters like buffer sizes, are configurable.

@section define_gap And what's with GAP?

@section Advertising

GAP is an other important protocol that allows a GATT client to discover devices, connect to them and to gather basic informations about a device. In Bluetooth all possible options related to GAP are passed as options to the @ref bluetoe::server type definition.

A Bluetoe GATT server will start to advertise by default and will restart to advertise after a client have disconnected to the server. Bluetoe will advertise the implemented services by default.

*/