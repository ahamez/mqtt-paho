#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include <mqtt/client.h>

#include "common.hh"

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

  const auto topic = std::string{"d/"} + std::string{id};

  mqtt::client cli{address, id};

  auto ssl_opts = mqtt::ssl_options{};
  ssl_opts.set_trust_store(root_crt);

  auto connection_options = mqtt::connect_options{};
  connection_options.set_keep_alive_interval(2);
  connection_options.set_clean_session(true);
  connection_options.set_will(mqtt::will_options{
    topic + "/status",
    std::string{"offline"},
    qos1,
    retained
  });
  connection_options.set_ssl(ssl_opts);

  std::cout << "Action default timeout " << cli.get_timeout().count() << " ms\n";
  cli.set_timeout(6s);

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

      for (auto i = 0ul;; ++i)
      {
        cli.publish(mqtt::make_message(
          topic,
          std::to_string(i),
          qos0,
          unretained
        ));

        std::cout << "Published " << i << " on " << topic << '\n';
        std::this_thread::sleep_for(1s);

        if (i % 10 == 0)
        {
          cli.publish(mqtt::make_message(
            topic + "/!",
            std::to_string(i) + "!",
            qos1,
            retained
          ));
          std::cout << "Published " << i << "! on " << topic << "/!\n";
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
