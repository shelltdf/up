namespace up::gui::platform::gtk {
int run(int argc, char** argv);
}

int main(int argc, char** argv) {
  return up::gui::platform::gtk::run(argc, argv);
}
