#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <cxxopts.hpp>

#include <mqtt/client.h>

#include "common.hh"

int
main(int argc, char** argv)
{
  using namespace std::chrono_literals;

  auto verify_server = false;
  auto id = std::string{};
  auto address = std::string{};
  auto payload_size = 0;
  auto period = 0;

  auto options = cxxopts::Options{argv[0]};
  options
    .positional_help("id address")
    .show_positional_help();

  options.add_options()
    ("help", "Help")
    ("id", "Identifier", cxxopts::value<std::string>(id))
    ("address", "Server address", cxxopts::value<std::string>(address))
    ("verify-server", "Use TLS", cxxopts::value<bool>(verify_server)->default_value("true"))
    ("crt", "Server certificate", cxxopts::value<std::string>())
    ("user", "User", cxxopts::value<std::string>())
    ("password", "Password", cxxopts::value<std::string>())
    ("payload-size", "Payload size", cxxopts::value<int>(payload_size)->default_value("20480"))
    ("period", "Period (ms)", cxxopts::value<int>(period)->default_value("1000"))
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

  const auto topic = std::string{"d/"} + id;

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
  cli.set_timeout(6s);

  auto payload = std::string(payload_size, 'x');

  while (true)
  {
    try
    {
      cli.connect(connection_options);

      cli.publish(mqtt::make_message(
        topic + "/status",
        std::string{"online"},
        qos1,
        retained
      ));

      cli.publish(mqtt::make_message(
        topic + "/info",
        std::string{"some infos"},
        qos1,
        unretained
      ));

      for (auto i = std::uint32_t{0};; ++i)
      {
        for (auto c = 0; c < 4; ++c)
        {
          payload[c] = reinterpret_cast<char*>(&i)[c];
        }

        cli.publish(mqtt::make_message(
          topic,
          payload,
          qos0,
          unretained
        ));

        std::this_thread::sleep_for(std::chrono::milliseconds{period});

        if (i % 10 == 0)
        {
          cli.publish(mqtt::make_message(
            topic + "/!",
            std::to_string(i) + "!",
            qos1,
            retained
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
