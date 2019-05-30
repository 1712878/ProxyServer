#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <string>
#include <fstream>
#include <afxsock.h>
#pragma warning (disable: 4996)
using namespace std;
int ConvertStringToChars(string str, char*& chars)
{
	int len = str.length();
	for (int i = 0; i < len; i++)
		chars[i] = str[i];
	chars[len] = '\0';
	return len;
}
void ConvertCharsToString(char* chars, string& str)
{
	str = "";
	int i = 0;
	while (chars[i] != '\0')
		str += chars[i++];
}
bool IsBlackList(const char* fileblack, char* receive)
{
	fstream fp; fp.open(fileblack);
	if (fp)
	{
		char temp[BUFSIZ];
		while (!fp.eof())
		{
			fp.getline(temp, BUFSIZ);
			if (strstr(receive, temp) != NULL)
			{
				fp.close();
				return true;
			}
		}
		fp.close();
		return false;
	}
}
string ReadHtml(const char* name)
{
	string result = "";
	fstream fp; fp.open(name);
	if (fp)
	{
		char temp[BUFSIZ];
		while (!fp.eof())
		{
			fp.getline(temp, BUFSIZ);
			result += temp;
		}
		fp.close();
	}
	return result;
}
string BuildResponse(bool flag)
{
	string res;
	string content;
	if (flag)
	{
		res = "HTTP/1.0 403 Forbidden\r\n";
		content = ReadHtml("Refuse.html");
	}
	else
	{
		res = "HTTP/1.0 200 OK\r\n";
		content = ReadHtml("accept.html");
	}

	res += "Content - Length : " + to_string(content.size()) + "\r\nConnection : close\r\nContent - Type : text / html; charset = UTF - 8\r\n\r\n";
	res += content;
	return res;
}
string ReadBlackList(string filename)
{
	fstream fp; fp.open(filename);
	string res;
	if (fp)
	{
		string temp;
		while (!fp.eof())
		{
			getline(fp, temp);
			res += temp;
		}
		fp.close();
	}
	return res;
}
bool CheckBlackList(string hostname, string hostblack)
{
	return hostblack.find(hostname) != hostblack.npos;
}
string GetDomainName(char* req)
{
	string res;
	ConvertCharsToString(req, res);
	size_t pos = res.find("Host");
	res.erase(0, pos + 6);
	pos = res.find("\r");
	res.erase(res.begin() + pos, res.end());
	/*pos = res.find(":");
	if(pos != res.npos)
		res.erase(res.begin() + pos, res.end());*/
	return res;
}
char* get_ip(char* host)
{
	struct hostent* hent;
	int iplen = 15; //XXX.XXX.XXX.XXX
	char* ip = (char*)malloc(iplen + 1);
	memset(ip, 0, iplen + 1);
	if ((hent = gethostbyname(host)) == NULL)
	{
		perror("Can't get IP");
		exit(1);
	}
	if (inet_ntop(AF_INET, (void*)hent->h_addr_list[0], ip, iplen) == NULL)
	{
		perror("Can't resolve host");
		exit(1);
	}
	return ip;
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
	CSocket Server;
	int port = 8888;
	/*cout << "Nhap port khoi tao Server: ";
	cin >> port;*/
	if (Server.Create(port, SOCK_STREAM, NULL) == FALSE)
	{
		//SOCK_STREAM: TCP, NULL: listen all card
		cout << "Khong the khoi tao Server!\n";
	}
	else
	{
		cout << "Khoi tao Server thanh cong\n";
		//Lang nghe Client
		if (Server.Listen(1) == FALSE)
		{
			cout << "Khong the lang nghe tren port nay!\n";
			Server.Close();
			return 0;
		}
		cout << "Dang doi Client ket noi....";
		string hostblack = ReadBlackList("blacklist.conf");
		//Tao 1 Connector de giao tiep Client
	ABC:
		CSocket Connector;
		if (Server.Accept(Connector))
		{
			cout << "\n\n=================Da co Client ket noi================\n";
			char* receive = new char[2048];
			string hostname;
			int len;

			len = Connector.Receive(receive, 2048, 0);
			receive[len] = '\0';
			hostname = GetDomainName(receive);

			cout << "=================Browser================\n" << receive << endl;
			if (CheckBlackList(hostname, hostblack) || hostname.find(":") != hostname.npos)
			{
				//send 403
				hostname = "HTTP/1.0 403 Forbidden\r\n";
				len = hostname.size();
				char* res = new char[len + 1];
				ConvertStringToChars(hostname, res);
				Connector.Send(res, len, 0);
				cout << "=================BlackList================\n";
				cout << res << endl;
				delete[] res; res = NULL;

			}
			else
			{
				CSocket P_Client;
				char* ip = new char[15];
				char* host = new char[hostname.size() + 1];
				ConvertStringToChars(hostname, host);
				ip = get_ip(host);

				P_Client.Create();
				P_Client.Connect(ip, 80);
				P_Client.Send(receive, strlen(receive), 0);

				char buf[BUFSIZ + 1];
				memset(buf, 0, sizeof(buf));
				int htmlstart = 0;
				int tmpres;
				char* htmlcontent;
				cout << "=================WebServer================\n";
				while ((tmpres = P_Client.Receive(buf, BUFSIZ, 0)) > 0)
				{
					if (htmlstart == 0)
					{
						/* Under certain conditions this will not work.
						* If the \r\n\r\n part is splitted into two messages
						* it will fail to detect the beginning of HTML content
						*/
						htmlcontent = strstr(buf, "\r\n\r\n");
						if (htmlcontent != NULL)
						{
							htmlstart = 1;
							htmlcontent += 4;
						}
					}
					else
					{
						htmlcontent = buf;
					}
					if (buf) {
						Connector.Send(buf, strlen(buf), 0);
						cout << buf;

					}
					memset(buf, 0, tmpres);
				}
				P_Client.Close();

				////delete[] ip;
				delete[] host;
			}

			delete[] receive; receive = NULL;

			Connector.Close();
			goto ABC;

		}
	}
	Server.Close();
	system("pause");
	return 0;
}