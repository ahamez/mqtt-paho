#include <chrono>
#include <iostream>
#include <string>
#include <thread>

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

  if (argc < 2)
  {
    std::cerr << argv[0] << " ID [BROKER]\n";
    return 1;
  }
  const auto id = argv[1];
  const auto address = argc >= 3
                     ? argv[2]
                     : "tcp://localhost:1883";
  const auto root_crt = argc >= 4
                      ? argv[3]
                      : "./root.crt";

  mqtt::client cli{address, id};

  auto ssl_opts = mqtt::ssl_options{};
  ssl_opts.set_trust_store(root_crt);

  auto cbk = callback{};
  cli.set_callback(cbk);

  auto connection_options = mqtt::connect_options{};
  connection_options.set_keep_alive_interval(3);
  connection_options.set_clean_session(false);
  // connection_options.set_clean_session(true);

  connection_options.set_ssl(ssl_opts);

  std::cout << "Action default timeout " << cli.get_timeout().count() << " ms\n";
  cli.set_timeout(6s);

  while (true)
  {
    try
    {
      cli.connect(connection_options);
      cli.subscribe(std::string{"d/#"});

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
