#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include <cxxopts.hpp>

#include <mqtt/client.h>

#include "common.hh"

struct callback
  : mqtt::callback
{
  void
  connected(const std::string& cause)
  override
  {
    std::cout << "[callback] connected, reason: " << cause << '\n';
  }

  void
  connection_lost(const std::string& cause)
  override
  {
    std::cout << "[callback] connection_lost, reason: " << cause << '\n';
  }
};

int
main(int argc, char** argv)
{
  using namespace std::chrono_literals;

  // if (argc < 2)
  // {
  //   std::cerr << argv[0] << " ID [BROKER]\n";
  //   return 1;
  // }
  // const auto id = argv[1];
  // const auto address = argc >= 3
  //                    ? argv[2]
  //                    : "tcp://localhost:1883";
  // const auto root_crt = argc >= 4
  //                     ? argv[3]
  //                     : "./root.crt";

  // mqtt::client cli{address, id};

  // auto ssl_opts = mqtt::ssl_options{};
  // ssl_opts.set_trust_store(root_crt);
  // ssl_opts.set_enable_server_cert_auth(false);


  // auto connection_options = mqtt::connect_options{};
  // connection_options.set_keep_alive_interval(3);
  // connection_options.set_clean_session(false);
  // // connection_options.set_clean_session(true);
  // connection_options.set_user_name("guest");
  // connection_options.set_password("Z2J7NNChYy");
  // connection_options.set_ssl(ssl_opts);

  // std::cout << "Action default timeout " << cli.get_timeout().count() << " ms\n";
  // cli.set_timeout(6s);

  auto verify_server = false;
  auto id = std::string{};
  auto address = std::string{};

  auto options = cxxopts::Options{argv[0]};
  options.add_options()
    ("help", "Help")
    ("id", "Identifier", cxxopts::value<std::string>(id))
    ("address", "Server address", cxxopts::value<std::string>(address))
    ("verify-server", "Use TLS", cxxopts::value<bool>(verify_server))
    ("crt", "Server certificate", cxxopts::value<std::string>())
    ("user", "User", cxxopts::value<std::string>())
    ("password", "Password", cxxopts::value<std::string>())
    ;

  options.parse_positional({"id", "address"});
  auto args = options.parse(argc, argv);

  if (args.count("help"))
  {
    std::cout << options.help({""}) << '\n';
    return 0;
  }

  auto connection_options = mqtt::connect_options{};
  auto ssl_opts = mqtt::ssl_options{};
  ssl_opts.set_trust_store(args["crt"].as<std::string>());
  ssl_opts.set_enable_server_cert_auth(verify_server);
  connection_options.set_ssl(ssl_opts);
  if (args.count("user"))
  {
    connection_options.set_user_name(args["user"].as<std::string>());
  }
  if (args.count("password"))
  {
    connection_options.set_password(args["password"].as<std::string>());
  }

  const auto topic = std::string{"s/"} + id;

  std::cout
    << "Identifer: " << id << '\n'
    << "Address: " << address << '\n'
    << "Certificate: " << args["crt"].as<std::string>() << '\n'
    << "Topic: " << topic << '\n'
    << "Verify server: " << std::boolalpha << verify_server << '\n'
    << '\n'
    ;

  connection_options.set_keep_alive_interval(2);
  connection_options.set_clean_session(true);
  connection_options.set_will(mqtt::will_options{
    topic + "/status",
    std::string{"offline"},
    qos1,
    retained
  });

  mqtt::client cli{address, id};
  auto cbk = callback{};
  cli.set_callback(cbk);
  cli.set_timeout(6s);

  while (true)
  {
    try
    {
      cli.connect(connection_options);
      cli.subscribe(std::string{"d/#"});

      cli.publish(mqtt::make_message(
        topic + "/status",
        std::string{"online"},
        qos1,
        retained
      ));

      while (true)
      {
        auto msg = std::make_shared<const mqtt::message>();
        if (cli.try_consume_message_for(&msg, 1s))
        {
          if (msg)
          {
            std::cout
              << msg->get_topic() << " -> "
              << msg->get_payload_str()
              << "[qos=" << msg->get_qos()
              << ", retained=" << std::boolalpha << msg->is_retained()
              << "]\n";
          }
          else
          {
            std::cout << "No message\n";
          }
        }
        else
        {
          // As we can't manually send a PINGREQ with paho (is it so sure?), some activity
          // is required to not trigger the timeout.
          // https://www.hivemq.com/blog/mqtt-essentials-part-10-alive-client-take-over.
          cli.publish(mqtt::make_message(
            "s/ping",
            std::string{"pong"},
            qos1,
            unretained
          ));
        }
      }
    }
    catch (const mqtt::exception& e)
    {
      std::cerr << "Error: " << e.what() << " [" << e.get_reason_code() << "]\n";
      std::this_thread::sleep_for(1s);
    }
  }

  return 0;
}
