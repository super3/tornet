#include <tornet/tornet_app.hpp>
#include <fc/ip.hpp>
#include <fc/fwd_impl.hpp>
#include <fc/exception.hpp>
#include <tornet/node.hpp>
#include <tornet/kad.hpp>
#include <tornet/chunk_service.hpp>
#include <tornet/name_service.hpp>

namespace tn {

  class tornet_app::impl {
    public:
      impl():_thread("tornet_app"){}
      fc::shared_ptr<node>           _node;
      fc::shared_ptr<name_service>   _name_service;
      fc::shared_ptr<chunk_service>  _chunk_service;
      fc::thread                     _thread;
  };

  tornet_app::tornet_app() {
  }

  tornet_app::~tornet_app() {
  }

  const tornet_app::ptr& tornet_app::instance() {
    static tornet_app::ptr inst( new tornet_app() );
    return inst;
  }

  void tornet_app::shutdown() {
    my->_name_service->shutdown();
    my->_name_service.reset();
    my->_chunk_service->shutdown();
    my->_chunk_service.reset();
    my->_node->shutdown();
    my->_node.reset();
  }

  void  tornet_app::configure( const config& c ) {
     if( !my->_thread.is_current() ) {
      my->_thread.async( [=]() { configure(c); } );
      return;
     }
     my->_node.reset(          new tn::node() );
     my->_node->init( c.data_dir, c.tornet_port );

     my->_chunk_service.reset( new tn::chunk_service( fc::path(c.data_dir.c_str()) / "chunks", my->_node ) );
     my->_name_service.reset(  new tn::name_service( fc::path(c.data_dir.c_str()) / "names", my->_node )   );
     
     for( uint32_t i = 0; i < c.bootstrap_hosts.size(); ++i ) {
       try {
           wlog( "Attempting to connect to %s", c.bootstrap_hosts[i].c_str() );
           fc::sha1 id = my->_node->connect_to( fc::ip::endpoint::from_string( c.bootstrap_hosts[i]) );
           wlog( "Connected to %s", fc::string(id).c_str() );
       } catch ( ... ) {
           wlog( "Unable to connect to node %s, %s", 
                 c.bootstrap_hosts[i].c_str(), fc::current_exception().diagnostic_information().c_str() );
       }
     }

     fc::sha1 target = my->_node->get_id();
     target.data()[19] += 1;
     slog( "Bootstrap, searching for self...", fc::string(my->_node->get_id()).c_str(), fc::string(target).c_str()  );
     tn::kad_search::ptr ks( new tn::kad_search( my->_node, target ) );
     ks->start();
     ks->wait();

     slog( "Results: \n" );
     //const std::map<fc::sha1,tn::host>&
     auto r = ks->current_results();
     auto itr  = r.begin(); 
     while( itr != r.end() ) {
       slog( "   distance: %s   node: %s  endpoint: %s", 
                fc::string(itr->first).c_str(), fc::string(itr->second.id).c_str(), fc::string(itr->second.ep).c_str() );
       ++itr;
     }
     my->_chunk_service->enable_publishing(true);
  }

  const fc::shared_ptr<node>&          tornet_app::get_node()const {
    return my->_node;
  }
  const fc::shared_ptr<name_service>&  tornet_app::get_name_service()const {
    return my->_name_service;
  }
  const fc::shared_ptr<chunk_service>& tornet_app::get_chunk_service()const {
    return my->_chunk_service;
  }


} // namespace tn
