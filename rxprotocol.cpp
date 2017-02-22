#include "rxprotocol.hpp"

#include "logger.hpp"
#include "config.hpp"
#include "task.hpp"
#include "utils.hpp"
#include "workspace.hpp"

#include "regex/regex.hpp"
#include "pstream.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <ctime>
#include <cmath>

namespace remexec {
	
using namespace std;
using namespace sinlib;
using namespace redi;

RXProtocol::RXProtocol()
{
	in = nullptr;
	out = nullptr;
}

RXProtocol::~RXProtocol()
{
}

void RXProtocol::response(string status, string info){
	*out << status << " " << info << endl << endl;
}

void RXProtocol::error(int code, string info){
	response(string("ERROR ") + to_string(code), info);
}

void RXProtocol::process(istream &in, ostream &out){
	this->in = &in;
	this->out = &out;

	Workspace workspace;

	string line;
	while (getline(in, line)){
		if (line.empty())
			continue;
		
		vector<string> args = regex_split(line, regex("\\s+"), 2);
		unordered_map<string, string> params;
		
		//command and argument (CMD arg)
		string cmd = args[0], arg = args.size() > 1 ? args[1] : "";
		
		//read parameters loop
		while (getline(in, line)){
			if (line.empty())
				break;

			vector<string> pars = regex_split(line, regex(":\\s*"), 2);
			if (pars.size() < 2){
				Log::warn("Non-parameter line, skipping: ", line);
				continue;
			}

			Log::debug("Parameter: ", pars[0], ": ", pars[1]);
			params[pars[0]] = pars[1];
		}

		if (cmd == "EXIT"){
			Log::debug("Client exit by protocol");
			return;
		}
		else if (cmd == "FILE"){
			auto pname = params.find("Name");
			auto psize = params.find("Size");
			if (pname != params.end() && psize != params.end()){
				string tmpath = workspace.getWorkdir();
				string name = regex_replace(pname->second, regex("\\.{1,2}/"), "");
				size_t size = stoull(psize->second);

				Log::debug("Appended file: ", name, " ", size);

				ofstream of(tmpath + name, ios::binary);
				bscopy(in, of, size);
				of.close();

				in.get(); in.get(); // '\n\n'

				response("OK");
			} else {
				error(3, "Name or Size parameter not set");
			}
		}
		else if (cmd == "EXEC"){
			string task = realpath(Config::getString(Config::TASK_DIR)) + "/" + arg;
			string tasktmp = workspace.getWorkdir();
			
			Log::debug("Task binary: ", task);
			Log::debug("Task tmp: ", tasktmp);

			if (path_exists(task)){
				Task t(arg, task, tasktmp);
				response("OK");
				t.run(out);
			} else {
				error(2, string("Task not found: ") + arg);
			}
		}
		else {
			error(1, string("Unknown command: ") + cmd);
		}
		
	}

	Log::debug("Client exit by end of stream");
}

}
