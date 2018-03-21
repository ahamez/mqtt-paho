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

bool
ends_with(const std::string& full, const std::string& ending)
{
  if (full.length() >= ending.length())
  {
    return (full.compare(full.length() - ending.length(), ending.length(), ending) == 0);
  }
  else
  {
    return false;
  }
}

int
main(int argc, char** argv)
{
  using namespace std::chrono_literals;

  auto verify_server = false;
  auto id = std::string{};
  auto address = std::string{};
  auto show_payload = false;
  auto clean_session = true;

  auto options = cxxopts::Options{argv[0]};
  options
    .positional_help("ID ADDRESS TOPIC [TOPIC...]")
    .show_positional_help();

  options.add_options()
    ("help", "Help")
    ("id", "Identifier", cxxopts::value<std::string>(id))
    ("address", "Server address", cxxopts::value<std::string>(address))
    ("verify-server", "Use TLS", cxxopts::value<bool>(verify_server))
    ("crt", "Server certificate", cxxopts::value<std::string>())
    ("user", "User", cxxopts::value<std::string>())
    ("password", "Password", cxxopts::value<std::string>())
    ("topics", "Topics to subscribe to", cxxopts::value<std::vector<std::string>>())
    ("show-payload", "Show payload", cxxopts::value<bool>(show_payload)->default_value("false"))
    ("clean-session", "Clean session", cxxopts::value<bool>(clean_session)->default_value("true"))
    ;

  options.parse_positional({"id", "address", "topics"});
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
  const auto subscribed_topics = [&]
  {
    if (args.count("topics"))
    {
      return args["topics"].template as<std::vector<std::string>>();
    }
    else
    {
      return std::vector<std::string>{};
    }
  }();

  std::cout
    << "Identifer: " << id << '\n'
    << "Address: " << address << '\n'
    << "Certificate: " << (args.count("crt") ? args["crt"].as<std::string>() : "N/A") << '\n'
    << "Topic: " << topic << '\n'
    << "Verify server: " << std::boolalpha << verify_server << '\n'
    << "Show payload: " << std::boolalpha << show_payload << '\n'
    << "Clean session: " << std::boolalpha << clean_session << '\n'
    << "Subscribed topics:";

  for (const auto& topic : subscribed_topics)
  {
    std::cout << " " << topic;
  }
  std::cout << "\n\n";

  connection_options.set_keep_alive_interval(3600*10);
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
  cli.set_timeout(2s);

  while (true)
  {
    try
    {
      cli.connect(connection_options);
      for (const auto& topic : subscribed_topics)
      {
        // cli.subscribe(topic, qos0);
        cli.subscribe(topic, qos1);
      }

      cli.publish(mqtt::make_message(
        topic + "/status",
        std::string{"online"},
        qos1,
        retained
      ));

      while (true)
      {
        const auto msg = cli.consume_message();
        if (not msg)
        {
          std::cout << "No message\n";
          break;
        }
        else if (show_payload)
        {
          const auto payload = [&]
          {
            if (ends_with(msg->get_topic(), "/status"))
            {
              return msg->get_payload_str();
            }
            else if (ends_with(msg->get_topic(), "/info"))
            {
              return msg->get_payload_str();
            }
            else if (ends_with(msg->get_topic(), "/!"))
            {
              return msg->get_payload_str();
            }
            else
            {
              auto i = std::uint32_t{0};
              for (auto c = 0; c < 4; ++c)
              {
                reinterpret_cast<char*>(&i)[c] = msg->get_payload_str()[c];
              }
              return std::to_string(i);
            }
          }();

          std::cout
            << msg->get_topic() << " -> "
            << payload
            << " [qos=" << msg->get_qos()
            << ", retained=" << std::boolalpha << msg->is_retained()
            << "]\n";
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
