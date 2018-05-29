#ifndef TOOLKIT_BASHTK_H_
#define TOOLKIT_BASHTK_H_



#define CLIENT_LOGFILEPATH "/var/log/beegfs-client.log"

const std::string BASH                          = "/bin/bash";

const std::string SETUP_DIR                  = "/opt/beegfs/setup";
const std::string CONFIG_FILE_PATH           = SETUP_DIR +"/confs/config";
const std::string META_ROLE_FILE_PATH        = SETUP_DIR + "/info/meta_server";
const std::string STORAGE_ROLE_FILE_PATH     = SETUP_DIR + "/info/storage_server";
const std::string CLIENT_ROLE_FILE_PATH      = SETUP_DIR + "/info/clients";
const std::string MGMTD_ROLE_FILE_PATH       = SETUP_DIR + "/info/management";
const std::string IB_CONFIG_FILE_PATH        = SETUP_DIR + "/info/ib_nodes";

const std::string TMP_DIR                    = "/tmp/beegfs";
const std::string SETUP_CLIENT_WARNING       = TMP_DIR + "/beegfs-setup-client-warnings.tmp";
const std::string FAILED_NODES_PATH          = TMP_DIR + "/failedNodes";
const std::string FAILED_SSH_CONNECTIONS     = TMP_DIR + "/beegfs-setup-check-ssh.tmp";

const std::string SETUP_LOG_PATH             = "/var/log/beegfs-setup.log";

const std::string ADMON_GUI_PATH             = "/opt/beegfs/beegfs-admon-gui/beegfs-admon-gui.jar";

const std::string SCRIPT_CHECK_SSH_PARALLEL     = SETUP_DIR + "/setup.check_ssh_parallel.sh";
const std::string SCRIPT_CHECK_STATUS           = SETUP_DIR + "/setup.check_status.sh";
const std::string SCRIPT_CHECK_STATUS_PARALLEL  = SETUP_DIR + "/setup.check_status_parallel.sh";
const std::string SCRIPT_GET_REMOTE_FILE        = SETUP_DIR + "/setup.get_remote_file.sh";
const std::string SCRIPT_INSTALL                = SETUP_DIR + "/setup.install.sh";
const std::string SCRIPT_INSTALL_PARALLEL       = SETUP_DIR + "/setup.install_parallel.sh";
const std::string SCRIPT_UNINSTALL              = SETUP_DIR + "/setup.uninstall.sh";
const std::string SCRIPT_UNINSTALL_PARALLEL     = SETUP_DIR + "/setup.uninstall_parallel.sh";
const std::string SCRIPT_REPO_PARALLEL          = SETUP_DIR + "/setup.add_repo_parallel.sh";
const std::string SCRIPT_START                  = SETUP_DIR + "/setup.start.sh";
const std::string SCRIPT_START_PARALLEL         = SETUP_DIR + "/setup.start_parallel.sh";
const std::string SCRIPT_STOP                   = SETUP_DIR + "/setup.stop.sh";
const std::string SCRIPT_STOP_PARALLEL          = SETUP_DIR + "/setup.stop_parallel.sh";
const std::string SCRIPT_SYSTEM_INFO_PARALLEL   = SETUP_DIR + "/setup.get_system_info_parallel.sh";
const std::string SCRIPT_WRITE_CONFIG           = SETUP_DIR + "/setup.write_config.sh";
const std::string SCRIPT_WRITE_CONFIG_PARALLEL  = SETUP_DIR + "/setup.write_config_parallel.sh";
const std::string SCRIPT_WRITE_MOUNTS           = SETUP_DIR + "/setup.write_mounts.sh";



class bashtk
{
};

#endif /* TOOLKIT_BASHTK_H_ */
