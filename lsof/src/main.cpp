#include <boost/program_options.hpp>
#include <string>
#include <vector>
#include <list>
#include <iostream>
#include <sys/types.h>
#include <dirent.h>
#include <exception>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <fstream>

namespace po = boost::program_options;
typedef std::list<std::string> Res;

const std::string LABEL = " COMMAND    PID       USER    FD    TYPE   SIZE/OFF     NODE NAME";

#define COMMAND_OFF 8
#define PID_OFF     15
#define USER_OFF    24
#define FD_OFF      29
#define TYPE_OFF    37
#define SIZE_OFF    47
#define NODE_OFF    55
#define NAME_OFF    57

const std::string PROC_PATH = "/proc";

#define DBG(desc, msg) std::cout << "["#desc"] " << msg << "\n";

std::string filterByUser  = "";
int filterByPID           = 0;
std::string filterByFile  = "";

Res pidRead(std::string&& spid) {
  Res res;
  char buff[2048] = {};

  // Command
  std::ifstream command_fd(PROC_PATH + "/" + spid + "/" + "comm");
  std::string cmd; 
  command_fd >> cmd;
  command_fd.close();

  // PID
  int pid = std::stoi(spid);

  // USER
  std::ifstream status_fd(PROC_PATH + "/" + spid + "/" + "status");

  for (int i = 0; i < 8; i++)
    status_fd.getline(nullptr, 0);
  
  char _[256];
  status_fd.getline(_, 256);
  status_fd >> _;
  int uid = 0;
  status_fd >> uid;

  status_fd.close();

  // UID -> USER_NAME
  struct passwd* pwd = getpwuid(uid);
  std::string user_name(pwd->pw_name);

  // -u flag
  if (filterByUser != "" && filterByUser != user_name)
    return {};

  // ITERATE OVER fd/ DIR
  DIR* pid_fd_dir = opendir((PROC_PATH + "/" + spid + "/fd").c_str());

  dirent* dent;
  while ((dent = readdir(pid_fd_dir))) {
    std::string fd_link_path = PROC_PATH + "/" + spid + "/fd/" + dent->d_name;
    struct stat fd_stat;
    stat(fd_link_path.c_str(), &fd_stat);

    // fd 
    if (std::string(dent->d_name).find_first_not_of("0123456789") != std::string::npos)
      continue;
    int fd_num = std::stoi(dent->d_name);
    
    char ftype[5];

    switch (fd_stat.st_mode & S_IFMT) {
      case S_IFBLK:   sprintf(ftype, "%s", "BLK");      break;
      case S_IFCHR:   sprintf(ftype, "%s", "CHR");      break;
      case S_IFDIR:   sprintf(ftype, "%s", "DIR");      break;
      case S_IFREG:   sprintf(ftype, "%s", "REG");      break;
      case S_IFSOCK:  sprintf(ftype, "%s", "SOCK");     break;
      case S_IFIFO:   sprintf(ftype, "%s", "FIFO");     break;
    }

    // SIZE/OFF
    int fd_src_size = fd_stat.st_size;
    
    // NODE
    int fd_node = fd_stat.st_ino;

    // HARD_LINK
    char fname[1024];
    int fname_size = 0;
    if ((fname_size = readlink(fd_link_path.c_str(), fname, 1024)) == -1) {
      perror("readlink()");
      throw std::system_error(std::make_error_code(std::errc::io_error), "Bad readlink()");
    } 
    else
      fname[fname_size] = '\0';
    
    // -f flag 
    if (filterByFile != "" && filterByFile != fname)
      continue;

    sprintf(
      buff, 
      "%-9s %6d %9s %5d %7s %10d %8d %-2s", 
      cmd.c_str(),
      pid,
      user_name.c_str(),
      fd_num,
      ftype,
      fd_src_size,
      fd_node,
      fname
    );

    res.push_back(buff);

    memset(buff, '\0', 2048);
  }

  closedir(pid_fd_dir);

  DIR* pid_mem_dir = opendir((PROC_PATH + "/" + spid + "/").c_str());

  return res;
}

Res procRead() {
  Res res;
  DIR* proc = opendir(PROC_PATH.c_str());

  dirent* dent;

  while ((dent = readdir(proc)) && !errno) {
    if (dent->d_type != DT_DIR) 
      continue;
    
    std::string sname(dent->d_name);
    if (sname.find_first_not_of("0123456789") != std::string::npos)
      continue;

    // -p flag
    if (filterByPID != 0 && filterByPID != std::stoi(sname))
      continue;

    res.merge(pidRead(std::move(sname)));
  }

  closedir(proc);

  if (errno) {
    perror("Error while reading direntry");
    throw std::system_error(std::make_error_code(std::errc::io_error), "Bad readdir()");
  }

  return res;
}

void printRes(Res& out) {
  std::cout << LABEL << "\n";

  for (auto &res : out) {
    std::cout << res << "\n";
  }
}

int main(int argc, char** argv) {
  using namespace std;

  po::options_description desc;
  pid_t pid;
  string file;

  desc.add_options()
    ("help", "Show this help")
    ("user", po::value<pid_t>()->value_name("USER"), "Show only files used by specified `user`")
    ("process", po::value<pid_t>()->value_name("PID"), "Show only files used by process with pid=`PID`")
    ("file", po::value<string>()->value_name("PATH"), "Show only descriptors of file with `PATH` to this file")
  ;

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);

  if (vm.count("help")) {
    cout << desc << "\n";
    return 0;
  }

  if (vm.count("user"))
    filterByUser = vm["user"].as<string>();
  
  if (vm.count("process"))
    filterByPID = vm["process"].as<pid_t>();
  
  if (vm.count("file"))
    filterByFile = vm["file"].as<string>();

  Res res;
  try {
    res = procRead();
  } catch (std::exception& e) {
    std::cerr << e.what();
    return 1;
  }

  printRes(res);
  return 0;
}