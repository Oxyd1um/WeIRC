#include "gui.h"

#include <FL/Enumerations.H>

#include "util.h"

// ---- sizes (غيّرها هنا لتغيير شكل النافذة) ----
static const int MARGIN = 8;      // space around and between widgets
static const int ROW_H = 30;      // height of buttons / input / server menu
static const int BUTTON_W = 100;
static const int SIDE_W = 200;    // width of the Channels/Users column
static const int LABEL_H = 20;    // height of the "Channels" / "Users" titles
static const int MIN_W = 640;
static const int MIN_H = 400;

// Close button hides the window (ends the program). Pressing Esc must NOT quit.
static void close_cb(Fl_Widget* w, void*) {
    if (Fl::event() == FL_SHORTCUT && Fl::event_key() == FL_Escape) return;
    w->hide();
}

static Fl_Box* make_title(const char* text) {
    Fl_Box* box = new Fl_Box(0, 0, 10, LABEL_H, text);
    box->align(FL_ALIGN_LEFT | FL_ALIGN_INSIDE);
    box->labelfont(FL_HELVETICA_BOLD);
    return box;
}

MainWindow::MainWindow() : Fl_Double_Window(900, 600, "WeIRC") {
    // Positions are all 0 here: layout() sets the real ones.
    server_choice_ = new Fl_Choice(0, 0, 10, ROW_H);
    connect_button_ = new Fl_Button(0, 0, BUTTON_W, ROW_H, "Connect");
    disconnect_button_ = new Fl_Button(0, 0, BUTTON_W, ROW_H, "Disconnect");

    chat_ = new Fl_Text_Display(0, 0, 10, 10);
    chat_buffer_ = new Fl_Text_Buffer();  // lives as long as the program, never freed
    chat_->buffer(chat_buffer_);
    chat_->textfont(FL_COURIER);
    chat_->textsize(14);
    chat_->wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);

    channels_label_ = make_title("Channels");
    channels_ = new Fl_Hold_Browser(0, 0, 10, 10);
    users_label_ = make_title("Users");
    users_ = new Fl_Browser(0, 0, 10, 10);

    input_ = new Fl_Input(0, 0, 10, ROW_H);
    send_button_ = new Fl_Button(0, 0, BUTTON_W, ROW_H, "Send");
    end();

    connect_button_->callback(connect_cb, this);
    disconnect_button_->callback(disconnect_cb, this);
    send_button_->callback(send_cb, this);
    input_->callback(send_cb, this);
    input_->when(FL_WHEN_ENTER_KEY_ALWAYS);
    channels_->callback(channel_cb, this);

    callback(close_cb);
    size_range(MIN_W, MIN_H);
    layout();
    Fl::focus(input_);
}

// ---------------------------------------------------------------------
//  Layout: every widget position is computed from the window size.
//  This is what makes the window resize correctly.
// ---------------------------------------------------------------------
void MainWindow::resize(int x, int y, int w, int h) {
    Fl_Double_Window::resize(x, y, w, h);
    layout();
}

void MainWindow::layout() {
    const int W = w();
    const int H = h();

    // Top bar
    const int top_y = MARGIN;
    disconnect_button_->resize(W - MARGIN - BUTTON_W, top_y, BUTTON_W, ROW_H);
    connect_button_->resize(disconnect_button_->x() - MARGIN - BUTTON_W, top_y, BUTTON_W, ROW_H);
    server_choice_->resize(MARGIN, top_y, connect_button_->x() - 2 * MARGIN, ROW_H);

    // Bottom bar
    const int bottom_y = H - MARGIN - ROW_H;
    send_button_->resize(W - MARGIN - BUTTON_W, bottom_y, BUTTON_W, ROW_H);
    input_->resize(MARGIN, bottom_y, send_button_->x() - 2 * MARGIN, ROW_H);

    // Middle area = whatever is left between the two bars
    const int mid_y = top_y + ROW_H + MARGIN;
    int mid_h = bottom_y - MARGIN - mid_y;
    if (mid_h < 0) mid_h = 0;

    const int side_x = W - MARGIN - SIDE_W;
    chat_->resize(MARGIN, mid_y, side_x - 2 * MARGIN, mid_h);

    // Right column: Channels (40%) on top, Users (60%) below
    int lists_h = mid_h - 2 * LABEL_H - MARGIN;
    if (lists_h < 0) lists_h = 0;
    const int channels_h = lists_h * 2 / 5;
    const int users_h = lists_h - channels_h;

    int y = mid_y;
    channels_label_->resize(side_x, y, SIDE_W, LABEL_H);
    y += LABEL_H;
    channels_->resize(side_x, y, SIDE_W, channels_h);
    y += channels_h + MARGIN;
    users_label_->resize(side_x, y, SIDE_W, LABEL_H);
    y += LABEL_H;
    users_->resize(side_x, y, SIDE_W, users_h);

    redraw();
}

// ---------------------------------------------------------------------
//  Things the program tells the window to show
// ---------------------------------------------------------------------
void MainWindow::add_message(const std::string& text) {
    // Keep memory bounded: drop the oldest part of a very long chat.
    if (chat_buffer_->length() > 300000) chat_buffer_->remove(0, 100000);

    std::string line = "[" + timestamp() + "] " + text + "\n";
    chat_buffer_->append(line.c_str());
    chat_->insert_position(chat_buffer_->length());
    chat_->show_insert_position();
}

void MainWindow::set_users(const std::vector<std::string>& names) {
    users_->clear();
    for (const auto& name : names) users_->add(name.c_str());
}

void MainWindow::set_channels(const std::vector<std::string>& names, const std::string& current) {
    channels_->clear();
    for (size_t i = 0; i < names.size(); ++i) {
        channels_->add(names[i].c_str());
        if (names[i] == current) channels_->select(static_cast<int>(i) + 1);  // browser lines start at 1
    }
}

void MainWindow::set_servers(const std::vector<std::string>& names) {
    // Remember the selection so it does not jump when the list is rebuilt.
    const char* old = server_choice_->text();
    std::string previous = old ? old : "";

    server_choice_->clear();
    int select = 0;
    for (size_t i = 0; i < names.size(); ++i) {
        server_choice_->add(names[i].c_str(), 0, nullptr);
        if (names[i] == previous) select = static_cast<int>(i);
    }
    if (!names.empty()) server_choice_->value(select);
    server_choice_->redraw();
}

void MainWindow::set_status(const std::string& text) {
    copy_label(("WeIRC - " + text).c_str());  // copy_label: FLTK keeps its own copy
}

// ---------------------------------------------------------------------
//  Button / input callbacks
// ---------------------------------------------------------------------
void MainWindow::send_text() {
    std::string text = input_->value();
    if (text.empty()) return;
    input_->value("");
    Fl::focus(input_);
    if (on_input) on_input(text);
}

void MainWindow::send_cb(Fl_Widget*, void* self) {
    static_cast<MainWindow*>(self)->send_text();
}

void MainWindow::connect_cb(Fl_Widget*, void* self) {
    MainWindow* win = static_cast<MainWindow*>(self);
    const char* name = win->server_choice_->text();
    if (name && win->on_connect) win->on_connect(name);
}

void MainWindow::disconnect_cb(Fl_Widget*, void* self) {
    MainWindow* win = static_cast<MainWindow*>(self);
    if (win->on_disconnect) win->on_disconnect();
}

void MainWindow::channel_cb(Fl_Widget*, void* self) {
    MainWindow* win = static_cast<MainWindow*>(self);
    int line = win->channels_->value();   // 0 = nothing selected
    if (line > 0 && win->on_pick_channel) win->on_pick_channel(win->channels_->text(line));
}
