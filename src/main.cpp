#include "app.h"
#include "utils/utils.h"

int main(int argc, char *argv[]) {
  App app;
  if (argc > 1)
    app.openDocument(utils::uriToPath(argv[1]));
  app.run();
  return 0;
}
