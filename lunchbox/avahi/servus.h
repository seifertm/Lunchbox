
/* Copyright (c) 2014, Stefan.Eilemann@epfl.ch
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 2.1 as published
 * by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "../clock.h"
#include "../debug.h"
#include "../os.h"

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-client/publish.h>
#include <avahi-common/error.h>
#include <avahi-common/simple-watch.h>

#include <net/if.h>
#include <stdexcept>

namespace lunchbox
{
namespace avahi
{
class Servus : public detail::Servus
{
public:
    explicit Servus( const std::string& name )
        : _name( name )
        , _poll( avahi_simple_poll_new( ))
        , _client( 0 )
        , _browser( 0 )
        , _group( 0 )
        , _result( lunchbox::Servus::Result::PENDING )
        , _port( 0 )
        , _announcable( false )
        , _scope( lunchbox::Servus::IF_ALL )
    {
        if( !_poll )
            LBTHROW( std::runtime_error( "Can't setup avahi poll device" ));

        int error = 0;
        _client = avahi_client_new( avahi_simple_poll_get( _poll ),
                                    (AvahiClientFlags)(0), _clientCBS, this,
                                    &error );
        if( !_client )
            LBTHROW( std::runtime_error(
                         std::string( "Can't setup avahi client: " ) +
                         avahi_strerror( error )));
    }

    virtual ~Servus()
    {
        withdraw();
        endBrowsing();

        if( _client )
            avahi_client_free( _client );
        if( _poll )
            avahi_simple_poll_free( _poll );
    }

    lunchbox::Servus::Result announce( const unsigned short port,
                                       const std::string& instance ) final
    {
        _result = lunchbox::Servus::Result::PENDING;
        _port = port;
        if( instance.empty( ))
            _announce = getHostname();
        else
            _announce = instance;

        if( _announcable )
            _createServices();
        else
        {
            lunchbox::Clock clock;
            while( !_announcable &&
                   _result == lunchbox::Servus::Result::PENDING &&
                   clock.getTime64() < ANNOUNCE_TIMEOUT )
            {
                avahi_simple_poll_iterate( _poll, ANNOUNCE_TIMEOUT );
            }
        }

        return lunchbox::Servus::Result( _result );
    }

    void withdraw() final
    {
        _announce.clear();
        _port = 0;
        if( _group )
            avahi_entry_group_reset( _group );
    }

    bool isAnnounced() const final
    {
        return ( _group && !avahi_entry_group_is_empty( _group ));
    }

    lunchbox::Servus::Result beginBrowsing(
                                  const lunchbox::Servus::Interface addr ) final
    {
        _scope = addr;
        if( _browser )
            return lunchbox::Servus::Result( lunchbox::Servus::Result::PENDING);

        _instanceMap.clear();
        return _browse();
    }

    lunchbox::Servus::Result browse( const int32_t timeout ) final
    {
        _result = lunchbox::Servus::Result::PENDING;
        lunchbox::Clock clock;

        do
        {
            if( avahi_simple_poll_iterate( _poll, timeout ) != 0 )
            {
                _result = lunchbox::Servus::Result::POLL_ERROR;
                break;
            }
        }
        while( clock.getTime64() < timeout );

        if( _result != lunchbox::Servus::Result::POLL_ERROR )
            _result = lunchbox::Servus::Result::SUCCESS;

        return lunchbox::Servus::Result( _result );
    }

    void endBrowsing() final
    {
        if( _browser )
            avahi_service_browser_free( _browser );
        _browser = 0;
    }

    bool isBrowsing() const final { return _browser; }

    Strings discover( const lunchbox::Servus::Interface addr,
                      const unsigned browseTime ) final
    {
        const lunchbox::Servus::Result& result = beginBrowsing( addr );
        if( !result && result != lunchbox::Servus::Result::PENDING )
            return getInstances();

        LBASSERT( _browser );
        browse( browseTime );
        if( result != lunchbox::Servus::Result::PENDING )
            endBrowsing();
        return getInstances();
    }

private:
    const std::string _name;
    AvahiSimplePoll* _poll;
    AvahiClient* _client;
    AvahiServiceBrowser* _browser;
    AvahiEntryGroup* _group;
    int32_t _result;
    std::string _announce;
    unsigned short _port;
    bool _announcable;
    lunchbox::Servus::Interface _scope;

    lunchbox::Servus::Result _browse()
    {
        _result = lunchbox::Servus::Result::SUCCESS;

        _browser = avahi_service_browser_new( _client, AVAHI_IF_UNSPEC,
                                              AVAHI_PROTO_UNSPEC, _name.c_str(),
                                              0, (AvahiLookupFlags)(0),
                                              _browseCBS, this );
        if( _browser )
            return lunchbox::Servus::Result( _result );

        _result = avahi_client_errno( _client );
        LBWARN << "Failed to create browser: " << avahi_strerror( _result )
               << std::endl;
        return lunchbox::Servus::Result( _result );
    }

    // Client state change
    static void _clientCBS( AvahiClient*, AvahiClientState state,
                            void* servus )
    {
        ((Servus*)servus)->_clientCB( state );
    }

    void _clientCB( AvahiClientState state )
    {
        switch (state)
        {
        case AVAHI_CLIENT_S_RUNNING:
            _announcable = true;
            if( !_announce.empty( ))
                _createServices();
            break;

        case AVAHI_CLIENT_FAILURE:
            _result = avahi_client_errno( _client );
            LBWARN << "Client failure: " << avahi_strerror( _result )
                   << std::endl;
            avahi_simple_poll_quit( _poll );
            break;

        case AVAHI_CLIENT_S_COLLISION:
            // Can't setup client
            _result = EEXIST;
            avahi_simple_poll_quit( _poll );
            break;

        case AVAHI_CLIENT_S_REGISTERING:
            /* The server records are now being established. This might be
             * caused by a host name change. We need to wait for our own records
             * to register until the host name is properly esatblished. */
            LBUNIMPLEMENTED; // withdraw & _createServices ?
            break;

        case AVAHI_CLIENT_CONNECTING:
            /*nop*/;
        }
    }

    // Browsing
    static void _browseCBS( AvahiServiceBrowser*, AvahiIfIndex ifIndex,
                            AvahiProtocol protocol, AvahiBrowserEvent event,
                            const char* name, const char* type,
                            const char* domain, AvahiLookupResultFlags,
                            void* servus )
    {
        ((Servus*)servus)->_browseCB( ifIndex, protocol, event, name, type,
                                      domain );
    }

    void _browseCB( const AvahiIfIndex ifIndex, const AvahiProtocol protocol,
                    const AvahiBrowserEvent event, const char* name,
                    const char* type, const char* domain )
    {
        LBVERB << "Browse event " << int(event) << " for "
               << (name ? name : "none") << " type " <<  (type ? type : "none")
               << std::endl;
        switch( event )
        {
        case AVAHI_BROWSER_FAILURE:
            _result = avahi_client_errno( _client );
            LBWARN << "Browser failure: " << avahi_strerror( _result )
                   << std::endl;
            avahi_simple_poll_quit( _poll );
            break;

        case AVAHI_BROWSER_NEW:
            /* We ignore the returned resolver object. In the callback function
               we free it. If the server is terminated before the callback
               function is called the server will free the resolver for us. */
            if( !avahi_service_resolver_new( _client, ifIndex, protocol, name,
                                             type, domain, AVAHI_PROTO_UNSPEC,
                                             (AvahiLookupFlags)(0),
                                             _resolveCBS, this ))
            {
                _result = avahi_client_errno( _client );
                LBWARN << "Error creating resolver: "
                       << avahi_strerror( _result ) << std::endl;
                avahi_simple_poll_quit( _poll );
                break;
            }

        case AVAHI_BROWSER_REMOVE:
            _instanceMap.erase( name );
            break;

        case AVAHI_BROWSER_ALL_FOR_NOW:
        case AVAHI_BROWSER_CACHE_EXHAUSTED:
            _result = lunchbox::Result::SUCCESS;
            break;
        }
    }

    // Resolving
    static void _resolveCBS( AvahiServiceResolver* resolver,
                             AvahiIfIndex, AvahiProtocol,
                             AvahiResolverEvent event, const char* name,
                             const char*, const char*,
                             const char* host, const AvahiAddress*,
                             uint16_t, AvahiStringList *txt,
                             AvahiLookupResultFlags, void* servus )
    {
        ((Servus*)servus)->_resolveCB( resolver, event, name, host, txt );
    }

    void _resolveCB( AvahiServiceResolver* resolver,
                     const AvahiResolverEvent event, const char* name,
                     const char* host, AvahiStringList *txt )
    {
        // If browsing through the local interface,
        // consider only the local instances
        if( _scope == lunchbox::Servus::IF_LOCAL )
        {
            const std::string& hostStr( host );
            // host in "hostname.local" format
            const size_t pos = hostStr.find_last_of( "." );
            const std::string hostName = hostStr.substr( 0, pos );

            if( hostName != getHostname( ))
                return;
        }

        switch( event )
        {
        case AVAHI_RESOLVER_FAILURE:
            _result = avahi_client_errno( _client );
            LBWARN << "Resolver error: " << avahi_strerror( _result )
                   << std::endl;
            break;

        case AVAHI_RESOLVER_FOUND:
            {
                detail::ValueMap& values = _instanceMap[ name ];
                values[ "servus_host" ] = host;
                for( ; txt; txt = txt->next )
                {
                    const std::string entry(
                                reinterpret_cast< const char* >( txt->text ),
                                txt->size );
                    const size_t pos = entry.find_first_of( "=" );
                    const std::string key = entry.substr( 0, pos );
                    const std::string value = entry.substr( pos + 1 );
                    values[ key ] = value;
                }
            } break;
        }

        avahi_service_resolver_free( resolver );
    }

    // Announcing
    void _updateRecord() final
    {
        if( _announce.empty() || !_announcable )
            return;

        if( _group )
            avahi_entry_group_reset( _group );
        _createServices();
    }

    void _createServices()
    {
        if( !_group )
            _group = avahi_entry_group_new( _client, _groupCBS, this );
        else
            avahi_entry_group_reset( _group );

        if( !_group )
            return;

        AvahiStringList* data = 0;
        for( detail::ValueMapCIter i = _data.begin(); i != _data.end(); ++i )
            data = avahi_string_list_add_pair( data, i->first.c_str(),
                                               i->second.c_str( ));

        _result = avahi_entry_group_add_service_strlst(
            _group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC,
                (AvahiPublishFlags)(0), _announce.c_str(), _name.c_str(), 0, 0,
                _port, data );

        if( data )
            avahi_string_list_free( data );

        if( _result != lunchbox::Result::SUCCESS )
        {
            avahi_simple_poll_quit( _poll );
            return;
        }

        _result = avahi_entry_group_commit( _group );
        if( _result != lunchbox::Result::SUCCESS )
            avahi_simple_poll_quit( _poll );
    }

    static void _groupCBS( AvahiEntryGroup*, AvahiEntryGroupState state,
                           void* servus )
    {
        ((Servus*)servus)->_groupCB( state );
    }

    void _groupCB( const AvahiEntryGroupState state )
    {
        switch( state )
        {
        case AVAHI_ENTRY_GROUP_ESTABLISHED:
            break;

        case AVAHI_ENTRY_GROUP_COLLISION:
        case AVAHI_ENTRY_GROUP_FAILURE:
            _result = EEXIST;
            avahi_simple_poll_quit( _poll );
            break;

        case AVAHI_ENTRY_GROUP_UNCOMMITED:
        case AVAHI_ENTRY_GROUP_REGISTERING:
            /*nop*/ ;
        }
    }
};

}
}
