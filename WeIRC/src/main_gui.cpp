#include <csignal>
#include <functional>
#include <string>
#include <vector>
#include "app.h"
#include "gui.h"
static void run_on_ui(std::function<void()> task) {
    auto* heap_task = new std::function<void()>(std::move(task));
    Fl::awake(
        [](void* p) {
            auto* t = static_cast<std::function<void()>*>(p);
            (*t)();
            delete t;
        },
        heap_task);
}

int main() {
    std::signal(SIGPIPE, SIG_IGN);  
   Fl::lock();
   Fl::scheme("gtk+");
    App app;
    MainWindow window;
    app.print = [&window](const std::string& text) {
        run_on_ui([&window, text] { window.add_message(text); });
    };
    app.client.on_status = [&window](const std::string& text) {
        run_on_ui([&window, text] { window.set_status(text); });
    };
    app.client.on_users = [&window](const std::vector<std::string>& users) {
        run_on_ui([&window, users] { window.set_users(users); });
    };
    app.client.on_channels = [&window](const std::vector<std::string>& channels,
                                       const std::string& current) {
        run_on_ui([&window, channels, current] { window.set_channels(channels, current); });
    };
    app.on_servers_changed = [&app, &window] {
        auto names = app.servers.names();
        run_on_ui([&window, names] { window.set_servers(names); });
    };
    window.on_input = [&app, &window](const std::string& text) {
        if (!app.handle_input(text)) window.hide();
    };
    window.on_connect = [&app](const std::string& name) { app.handle_input("/connect " + name); };
    window.on_disconnect = [&app] { app.handle_input("/disconnect"); };
    window.on_pick_channel = [&app](const std::string& channel) {
        app.handle_input("/switch " + channel);
    };
    app.load();
   window.set_status("disconnected");
   window.show();
    int result = Fl::run();
    app.shutdown();
    return result;
}
