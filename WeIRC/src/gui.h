#pragma once
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Browser.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Hold_Browser.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>
#include <functional>
#include <string>
#include <vector>
class MainWindow : public Fl_Double_Window {
public:
std::function<void(const std::string&)> on_input;     
std::function<void(const std::string&)> on_connect;      
std::function<void()> on_disconnect;
std::function<void(const std::string&)> on_pick_channel;  
MainWindow();
void add_message(const std::string& text);
void set_users(const std::vector<std::string>& names);
void set_channels(const std::vector<std::string>& names, const std::string& current);
void set_servers(const std::vector<std::string>& names);
  void set_status(const std::string& text);
    void resize(int x, int y, int w, int h) override;
private:
   void layout();   
   void send_text();
    static void send_cb(Fl_Widget*, void* self);
    static void connect_cb(Fl_Widget*, void* self);
    static void disconnect_cb(Fl_Widget*, void* self);
   static void channel_cb(Fl_Widget*, void* self);
    Fl_Choice* server_choice_;
    Fl_Button* connect_button_;
   Fl_Button* disconnect_button_;
    Fl_Text_Display* chat_;
   Fl_Text_Buffer* chat_buffer_;
    Fl_Box* channels_label_;
   Fl_Hold_Browser* channels_;
    Fl_Box* users_label_;
    Fl_Browser* users_;
    Fl_Input* input_;
    Fl_Button* send_button_;
};
