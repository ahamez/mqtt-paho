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
  if (args.count("crt"))
  {
    ssl_opts.set_trust_store(args["crt"].as<std::string>());
  }
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
    << "Certificate: " << (args.count("crt") ? args["crt"].as<std::string>() : "N/A") << '\n'
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
