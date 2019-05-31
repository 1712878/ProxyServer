#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <afxsock.h>
#include <direct.h>
#define BSIZE 4096
#pragma warning (disable: 4996)
using namespace std;

const string FILE_BLACKLIST = "blacklist.conf";
const string CACHE_FOLDER = "Cache//";
const string FILE_CACHE_LOG = "cachelog.conf";
vector<string> hostBlack;
vector<string> cacheDomain;

typedef struct ReceiveInfo
{
	string method;
	string host;
	string page;
};
ReceiveInfo getReceiveInfo(string request)
{
	// GET http://example.com/img/1.jpg HTTP/1.1/r/nHost: example.com\r\n...
	ReceiveInfo res;
	size_t pos = request.find(" ");
	res.method = request.substr(0, pos);//GET
	request.erase(0, pos+1);

	pos = request.find(" HTTP/1.");
	res.page = request.substr(0, pos);//http://example.com/img/1.jpg

	pos = request.find("Host");
	request.erase(0, pos + 6);
	pos = request.find("\r\n");
	res.host = request.substr(0, pos);//example.com

	return res;
}
bool isSupport(ReceiveInfo ri)
{
	return (ri.method == "GET" || ri.method == "POST") && ri.host.find(":") == string::npos;
}

vector<string> readFile(string filename)
{
	fstream fp; fp.open(filename);
	vector<string> res;
	if (fp)
	{
		string temp;
		while (!fp.eof())
		{
			getline(fp, temp);
			res.push_back(temp);
		}
		fp.close();
	}
	return res;
}
bool isBlackList(string hostname)
{
	int size = hostBlack.size();
	size_t npos = hostname.npos;
	for (int i = 0; i < size; i++)
	{
		if (hostBlack[i].find(hostname) != npos || hostname.find(hostBlack[i]) != npos)
		{
			return true;
		}
	}
	return false;
}
void changeType(string& src) {
	string s = "\/*:\"<>|?";
	for (int i = 0; i < src.size(); i++) {
		if (s.find(src[i]) != string::npos) {
			src[i] = '_';
		}
	}
}
string getCache(string host, string page)
{
	int size = cacheDomain.size();
	string res = "";
	for (int i = 0; i < size; i++)
	{
		if (cacheDomain[i] == host)
		{
			changeType(page);
			string filename = CACHE_FOLDER + host + "//" + page;
			ifstream fp(filename, ios::binary);
			if (fp)
			{
				fp.seekg(0, fp.end);
				size_t pos = fp.tellg();
				fp.seekg(0, fp.beg);
				char* buf = new char[pos];
				memset(buf, 0, pos);
				fp.read(buf, sizeof(char) * pos);
				res = string(buf, pos);
				delete[] buf;


				/*char* buf = new char[BSIZE];
				while (!fp.eof())
				{
					memset(buf, 0, BSIZE);
					fp.read(buf, BSIZE);
					res += string(buf);
				}
				delete[] buf;
				fp.close();*/
			}
			break;
		}
	}
	return res;
}
void saveFileDomainName(string filename, string hostname)
{
	mkdir((CACHE_FOLDER + hostname).c_str());
	fstream fp;
	fp.open(filename, ios::app);
	fp << hostname << endl;
	fp.close();
}
void saveFileCache(string host, string page, string buf)
{
	changeType(page);
	string filename = CACHE_FOLDER + host + "//" + page;
	ofstream fp(filename, ios::binary);
	if (fp)
	{
		fp.write(buf.c_str(), buf.size());
		fp.close();
	}
}
bool findHostList(string host)
{
	int size = cacheDomain.size();
	for (int i = 0; i < size; i++)
	{
		if (host == cacheDomain[i])
			return true;
	}
	return false;
}
string getIP(const char* host)
{
	struct hostent* hent;
	string res ="";
	
	char* ip = new char[16];
	memset(ip, 0, 16);
	if ((hent = gethostbyname(host)) == NULL)
	{
		perror("Can't get IP");
		goto getIPEnd;
	}
	if (inet_ntop(AF_INET, (void*)hent->h_addr_list[0], ip, 15) == NULL)
	{
		perror("Can't resolve host");
	}
getIPEnd:
	res = string(ip);
	delete[] ip;
	return res;
}
DWORD WINAPI ProcessClient(LPVOID lp)
{
	//cout << "Da co client ket noi\n";
	SOCKET* hConnected = (SOCKET*)lp;
	CSocket Proxy_WebClient;
	//Chuyen ve lai CSocket
	Proxy_WebClient.Attach(*hConnected);

	int len;
	char* buf = new char[BSIZE];
	memset(buf, 0, BSIZE);
	string receive = "";

	while ((len = Proxy_WebClient.Receive(buf, BSIZE, 0)) > 0)
	{
		receive += (string)buf;
		if (receive.find("\r\n\r\n") != string::npos)
			break;
		memset(buf, 0, BSIZE);
	}
	
	ReceiveInfo ri = getReceiveInfo(receive);
	string hostname = ri.host;
	string page = ri.page;
	string respone;
	//cout << "=================Browser================\n" << receive << endl;

	if (isBlackList(hostname) || !isSupport(ri))
	{
		//send 403
		respone = "HTTP/1.0 403 Forbidden\r\n";
		Proxy_WebClient.Send(respone.c_str(), respone.size(), 0);

	}
	else if ((respone = getCache(hostname, page)) != "")
	{
		cout << "Da co client ket noi\n";
		cout << "=================Browser================\n" << receive << endl;
		//send cache saved
		Proxy_WebClient.Send(respone.c_str(), respone.size(), 0);
	}
	else
	{
		cout << "Da co client ket noi\n";
		cout << "Hostname: " << hostname << endl;
		string ip = getIP(hostname.c_str());

		if (ip != "")
		{
			cout << "IP: " << ip << endl;
			//Tao 1 cong de nhan DL tu WebServer gui ve
			CSocket Proxy_WebServer;
			Proxy_WebServer.Create();
			if (Proxy_WebServer.Connect(ip.c_str(), 80) == FALSE)
			{
				//send 404
				respone = "HTTP/1.0 404 NotFound\r\n";
				Proxy_WebClient.Send(respone.c_str(), respone.size(), 0);
			}
			else
			{
				// FILE :) 
				if (!findHostList(hostname))
				{
					cacheDomain.push_back(hostname);
					saveFileDomainName(FILE_CACHE_LOG, hostname);
				}

				Proxy_WebServer.Send(receive.c_str(), receive.size() + 1, 0);
				memset(buf, 0, BSIZE);
				receive = "";
				while ((len = Proxy_WebServer.Receive(buf, BSIZE, 0)) > 0)
				{
					receive += string(buf, len);
					memset(buf, 0, len);
					/*if (buf[len - 1] == 0 && buf[len - 2] == 0)
						break;*/
				}
				Proxy_WebClient.Send(receive.c_str(), receive.size(), 0);
				saveFileCache(hostname, page, receive);

			}

			Proxy_WebServer.Close();
		}
	}

	delete[] buf;
	delete hConnected;
	Proxy_WebClient.Close();

}

int main()
{
	//Kiem tra goi MFC
	if (!AfxWinInit(::GetModuleHandle(NULL), NULL, ::GetCommandLine(), 0))
	{
		cout << "Khong the su dung goi MFC\n";
		return 0;
	}
	if (AfxSocketInit() == FALSE)
	{
		cout << "Khong the khoi tao Socket\n";
		return 0;
	}
	//Khoi tao Server
	CSocket ProxyServer;
	int port = 8888;
	/*cout << "Nhap port khoi tao Server: ";
	cin >> port;*/
	if (ProxyServer.Create(port, SOCK_STREAM, NULL) == FALSE)
	{
		//SOCK_STREAM: TCP, NULL: listen all card
		cout << "Khong the khoi tao Server!\n";
	}
	else
	{
		cout << "Khoi tao Server thanh cong\n";
		//Doc blacklist
		hostBlack = readFile(FILE_BLACKLIST);
		cacheDomain = readFile(FILE_CACHE_LOG);
		mkdir(CACHE_FOLDER.c_str());
		//Tao 1 Cong giao tiep voi Browser
		CSocket Proxy_WebClient;
		//*
		DWORD threadID;
		HANDLE threadStatus;
		while (1)
		{
			//cout << "Dang doi Client ket noi....";
			ProxyServer.Listen();
			ProxyServer.Accept(Proxy_WebClient);
			//Khoi tao con tro Socket
			SOCKET* hConnected = new SOCKET();
			//Chuyển đỏi CSocket thanh Socket
			*hConnected = Proxy_WebClient.Detach();
			//Khoi tao thread tuong ung voi moi client Connect vao server.
			//Nhu vay moi client se doc lap nhau, khong phai cho doi tung client xu ly rieng
			//nthread++;
			threadStatus = CreateThread(NULL, 0, ProcessClient, hConnected, 0, &threadID);
		}
	}
	ProxyServer.Close();
	system("pause");
	return 0;
}