#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <afxsock.h>
#include <direct.h>
#define BSIZE 4096
#pragma warning (disable: 4996)
//int nthread = 0;
using namespace std;
vector<string> hostBlack;
vector<string> cacheDomain;

vector<string> ReadFile(string filename)
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

bool IsBlackList(string hostname)
{
	if (hostname.find(":443") != string::npos)
		return true;

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
string GetDomainName(string res)
{
	size_t pos = res.find("Host");
	res.erase(0, pos + 6);
	pos = res.find("\r\n");
	res = res.substr(0, pos);
	/*pos = res.find(":443");
	if(pos != res.npos)
		res.erase(res.begin() + pos, res.end());*/
	return res;
}
string GetPage(string str)
{
	size_t pos = str.find("GET");
	str.erase(0, pos + 4);
	pos = str.find(" HTTP/1.");
	str = str.substr(0, pos);
	return str;
}
void changeType(string& src) {
	string s = "\/*:\"<>|?";
	for (int i = 0; i < src.size(); i++) {
		if (s.find(src[i]) != string::npos) {
			src[i] = '_';
		}
	}
}
string GetCache(string host, string page)
{
	int size = cacheDomain.size();
	string res = "";
	for (int i = 0; i < size; i++)
	{
		if (cacheDomain[i] == host)
		{
			changeType(page);
			string filename = "cache//" + host + "//" + page + ".txt";
			fstream fp(filename, ios::in, ios::binary);
			if (fp)
			{
				fp.seekg(0, fp.end);
				int pos = fp.tellg();
				fp.seekg(0, fp.beg);
				char* buf = new char[pos + 1];
				fp.read(buf, sizeof(char) * pos);
				res = (string)buf;
				delete[] buf;
			}
			break;
		}
	}
	return res;
}
void SaveFileDomainName(string filename, string hostname)
{
	mkdir(("./cache/" + hostname).c_str());
	fstream fp;
	fp.open(filename, ios::app);
	fp << hostname << endl;
	fp.close();
}
void SaveFileCache(string host, string page, string buf)
{
	changeType(page);
	string filename = "cache//" + host + "//" + page + ".txt";
	fstream fp;
	fp.open(filename, ios::app);
	if (fp)
	{
		fp << buf;
		fp.close();
	}
}
bool FindHostList(string host)
{
	int size = cacheDomain.size();
	for (int i = 0; i < size; i++)
	{
		if (host == cacheDomain[i])
			return true;
	}
	return false;
}
bool get_ip(const char* host, char*& ip)
{
	struct hostent* hent;
	int iplen = 15; //XXX.XXX.XXX.XXX
	//char* ip = (char*)malloc(iplen + 1);
	memset(ip, 0, iplen + 1);
	if ((hent = gethostbyname(host)) == NULL)
	{
		perror("Can't get IP");
		return false;
	}
	if (inet_ntop(AF_INET, (void*)hent->h_addr_list[0], ip, iplen) == NULL)
	{
		perror("Can't resolve host");
		return false;
	}
	return true;
}
DWORD WINAPI ProcessClient(LPVOID lp)
{
	cout << "Da co client ket noi\n";
	//int t = nthread;
	//cout << "\n\nSTART-------------- " << t << " -------------------------\n";
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

	string hostname = GetDomainName(receive);
	string page = GetPage(receive);
	string respone;
	//cout << "=================Browser================\n" << receive << endl;

	if (IsBlackList(hostname))
	{
		//send 403
		respone = "HTTP/1.0 403 Forbidden\r\n";
		Proxy_WebClient.Send(respone.c_str(), respone.size(), 0);

	}
	else if ((respone = GetCache(hostname, page)) != "")
	{
		cout << "=================Browser================\n" << receive << endl;

		//send cache saved
		//respone = GetCache(hostname, page);
		Proxy_WebClient.Send(respone.c_str(), respone.size(), 0);
		//cout << respone << endl;
		changeType(page);
		string filename = "cache//" + hostname + "//" + page + "_respone.txt";
		fstream fp;  fp.open(filename, ios::out);
		fp << respone;
		fp.close();
	}
	else
	{
		cout << "Hostname: " << hostname << endl;
		char* ip = new char[16];
		if (get_ip(hostname.c_str(), ip))
		{
			cout << "IP: " << ip << endl;

			//Tao 1 cong de nhan DL tu WebServer gui ve
			CSocket Proxy_WebServer;
			Proxy_WebServer.Create();
			if (Proxy_WebServer.Connect(ip, 80) == FALSE)
			{
				//send 404
				respone = "HTTP/1.0 404 NotFound\r\n";
				Proxy_WebClient.Send(respone.c_str(), respone.size(), 0);
			}
			else
			{
				// FILE :) 
				if (!FindHostList(hostname))
				{
					cacheDomain.push_back(hostname);
					SaveFileDomainName("logcache.conf", hostname);
				}

				Proxy_WebServer.Send(receive.c_str(), receive.size() + 1, 0);
				memset(buf, 0, BSIZE);
				receive = "";
				while ((len = Proxy_WebServer.Receive(buf, BSIZE, 0)) > 0)
				{
					//Proxy_WebClient.Send(buf, len, 0);
					receive += string(buf, len);
					memset(buf, 0, len);
				}
				Proxy_WebClient.Send(receive.c_str(), receive.size(), 0);
				SaveFileCache(hostname, page, receive);
			}

			Proxy_WebServer.Close();
		}
		delete[] ip;
	}

	delete[] buf;
	delete hConnected;
	Proxy_WebClient.Close();
	//cout << "\n\nEND-------------- " << t << " -------------------------\n";

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
		hostBlack = ReadFile("blacklist.conf");
		cacheDomain = ReadFile("logcache.conf");
		mkdir(string("cache").c_str());
		//Tao 1 Cong giao tiep voi Browser
		CSocket Proxy_WebClient;
		//*
		DWORD threadID;
		HANDLE threadStatus;
		while (1)
		{
			cout << "Dang doi Client ket noi....";
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