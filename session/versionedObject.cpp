
/* Copyright (c) 2006-2007, Stefan Eilemann <eile@equalizergraphics.com> 
   All rights reserved. */

#include <test.h>

#include <eq/client/nodeFactory.h>
#include <eq/net/connection.h>
#include <eq/net/init.h>
#include <eq/net/pipeConnection.h>
#include <eq/net/session.h>
#include <eq/net/object.h>

#include <iostream>

using namespace eq::base;
using namespace std;

eq::NodeFactory* eq::createNodeFactory() { return new eq::NodeFactory; }

class TestObject : public eq::net::Object
{
public:
    TestObject() : 
            Object( eq::net::Object::TYPE_VERSIONED_CUSTOM ),
            version(42) {}

protected:
    int version;

    const void* getInstanceData( uint64_t* size )
        {
            return pack( size );
        }

    const void* pack( uint64_t* size )
        {
            *size = sizeof( version );
            return &version;
        }

    void unpack( const void* data, const uint64_t size )
        {
            TEST( size == sizeof( version ));

            int newVersion = *(int*)data;
            TEST( newVersion+1 == version );
            version = newVersion;
        }
};

class Session : public eq::net::Session
{
    eq::net::Object* instanciateObject( const uint32_t type, const void* data, 
                                        const uint64_t dataSize )
        {
            if( type == eq::net::Object::TYPE_VERSIONED_CUSTOM )
                return new TestObject();

            return eq::net::Session::instanciateObject( type, data, dataSize );
        }
};

RefPtr<eq::net::Connection> connection;
volatile uint32_t         testID = EQ_ID_INVALID;

class ServerThread : public eq::base::Thread
{
protected:
    virtual void* run()
        {
            RefPtr<eq::net::Node> server    = new eq::net::Node;
            RefPtr<eq::net::Node> nodeProxy = new eq::net::Node;

            TEST( server->listen( ));

            eq::net::PipeConnection* pipeConnection = 
                (eq::net::PipeConnection*)connection.get();

            TEST( server->connect( nodeProxy, pipeConnection->getChildEnd( )));
            TEST( nodeProxy->isConnected( ));
                        
            Session session;
            TEST( server->mapSession( server, &session, "foo" ));

            while( testID == EQ_ID_INVALID ); // spin for test object

            RefPtr<eq::net::Object> object = session.getObject( testID );
            TEST( dynamic_cast<TestObject*>(object.get()) );
                
            TestObject* testObject = (TestObject*) object.get();
            TEST( testObject );
            TEST( testObject->getID() == testID );

            sleep(1);

            TEST( server->stopListening( ));
            TEST( server->getRefCount() == 1 );
            TEST( nodeProxy->getRefCount() == 1 );
            
            return EXIT_SUCCESS;
        }
};

int main( int argc, char **argv )
{
    eq::net::init( argc, argv );

    connection = new eq::net::PipeConnection();
    TEST( connection->connect( ));

    ServerThread server;
    server.start();

    RefPtr<eq::net::Node> node        = new eq::net::Node;
    RefPtr<eq::net::Node> serverProxy = new eq::net::Node;

    TEST( node->listen( ));
    TEST( node->connect( serverProxy, connection ));
    TEST( serverProxy->isConnected( ));

    Session session;
    TEST( node->mapSession( serverProxy, &session, "foo" ));
    
    TestObject obj;
    session.registerObject( &obj );

    testID = obj.getID();
    TEST( testID != EQ_ID_INVALID );

    sleep(1);

    TEST( server.join( ));
    TEST( node->stopListening( ));
    TEST( connection->getRefCount() == 1 );
    TEST( node->getRefCount() == 1 );
    TEST( serverProxy->getRefCount() == 1 );
}
