#include "curl.h"
#include "Share.h"
#include "GetData.hpp"
#include "MulGetData.h"
#include <stdio.h>
#include <string> 
#include <iostream>
#include <sstream>  
#include <vector>
#include <locale>
#include <locale.h>
#include <iomanip>
#include <fstream>
#include <thread>
#include <stack>
#include <map>
#include <ctime>
#include <mutex>
#include <string>
using namespace std;

// get share spy from 20160901 to 20180430 and slice later
// calculated its price and return
void MulGetspyAlsoGetCrumb(Share& spy, string& sCrumb, string& sCookies) {
	string startTime = getTimeinSeconds("2016-09-01T16:00:00");
	string endTime = getTimeinSeconds("2018-04-30T16:00:00");
	spy.SetTicker("spy");
	struct MemoryStruct data;
	data.memory = NULL;
	data.size = 0;

	FILE *fp;
	CURL * handle;
	handle = curl_easy_init();
	CURLcode result;
	if (handle)
	{
		curl_easy_setopt(handle, CURLOPT_URL, "https://finance.yahoo.com/quote/AMZN/history");
		curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0);
		curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0);
		curl_easy_setopt(handle, CURLOPT_COOKIEJAR, "cookies.txt");
		curl_easy_setopt(handle, CURLOPT_COOKIEFILE, "cookies.txt");
		curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_data2);
		curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)&data);

		result = curl_easy_perform(handle);

		if (result != CURLE_OK)
		{

			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(result));
			//return 1;
		}

		char cKey[] = "CrumbStore\":{\"crumb\":\"";
		char *ptr1 = strstr(data.memory, cKey);
		char *ptr2 = ptr1 + strlen(cKey);
		char *ptr3 = strstr(ptr2, "\"}");
		if (ptr3 != NULL)
			*ptr3 = NULL;

		sCrumb = ptr2;
		fp = fopen("cookies.txt", "r");
		char cCookies[100];
		if (fp) {
			while (fscanf(fp, "%s", cCookies) != EOF);
			fclose(fp);
		}
		else
			cerr << "cookies.txt does not exists" << endl;


		sCookies = cCookies;
		free(data.memory);
		data.memory = NULL;
		data.size = 0;

		string urlA = "https://query1.finance.yahoo.com/v7/finance/download/";
		string symbol = spy.GetTicker();
		string urlB = "?period1=";
		string urlC = "&period2=";
		string urlD = "&interval=1d&events=history&crumb=";
		string url = urlA + symbol + urlB + startTime + urlC + endTime + urlD + sCrumb;
		const char * cURL = url.c_str();
		const char * cookies = sCookies.c_str();
		curl_easy_setopt(handle, CURLOPT_COOKIE, cookies);   // Only needed for 1st stock
		curl_easy_setopt(handle, CURLOPT_URL, cURL);
		curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_data2);
		curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)&data);
		result = curl_easy_perform(handle);


		if (result != CURLE_OK)
		{

			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(result));
			//return 1;
		}

		stringstream sData;
		sData.str(data.memory);
		string sValue, sDate;
		double dValue = 0;
		string line;
		map<Date, double> curprice; // store date with price
		stack<Date> dates;
		stack<double> adj;
		getline(sData, line);
		while (getline(sData, line)) {
			//cout << line << endl;
			sDate = line.substr(0, line.find_first_of(','));
			line.erase(line.find_last_of(','));
			sValue = line.substr(line.find_last_of(',') + 1);
			dValue = strtod(sValue.c_str(), NULL);
			tm t{};
			istringstream d(sDate);
			d >> get_time(&t, "%Y-%m-%d");
			int year = t.tm_year+1900;
			int mon = t.tm_mon+1;
			int day = t.tm_mday;
			Date dd(year, mon, day);
			dates.push(dd);
			adj.push(dValue);
		}

		while (!dates.empty()) {
			curprice[dates.top()] = adj.top();
			dates.pop();
			adj.pop();
		}

		// 
		spy.SetPrice(curprice);
		spy.SetStartDate((curprice.begin())->first);
		spy.SetEndDate((--curprice.end())->first);

		// calculate return
		std::map<Date, double> ret;
		for (auto i = curprice.begin(); i != --curprice.end(); i++) {
			auto j = i;
			j++; // j = i+1
			ret[j->first] = (j->second - i->second) / i->second;
		}
		spy.SetReturn(ret);

		free(data.memory);
		data.size = 0;
		cout << spy.GetTicker();
	}
	else
	{
		fprintf(stderr, "Curl init failed!\n");
	}
	curl_easy_cleanup(handle);
}

// read 500 stock eps from csv
void MulAllStock(map<string, Stock>& all_stocks) {

	ifstream file("SP500.csv");
	string line;
	if (!file.is_open()) {
		cerr << "Load SP500data.csv Failed" << endl;
		system("pause");
	}

	while (getline(file, line)) {
		istringstream ss(line);
		string token;
		Stock temp;

		getline(ss, token, ',');
		temp.SetTicker(token); // ticker

		getline(ss, token, ',');
		string date = token; // date

		getline(ss, token, ',');
		double est = stod(token); // estimated eps, string to double

		getline(ss, token, ','); // actual eps
		temp.SetSurprise(100 * (stod(token) - est) / est); // calculate surprise -100-100

		getline(ss, token, ',');
		int y = stoi(token); // string to int: year

		getline(ss, token, ',');
		int m = stoi(token); // month

		getline(ss, token, ',');
		int d = stoi(token); // date

		Date rep(y, m, d);
		temp.SetReportDate(rep);
		all_stocks[temp.GetTicker()] = temp;
	}

	cout << "Reading Mission Completed" << endl;
}

// download price using spy crumb and calculate its return
void Muldownload(Stock& stock, string sCrumb, string sCookies) {
	//barrier.lock();
	struct MemoryStruct data;
	data.memory = NULL;
	data.size = 0;
	FILE *fp;
	CURL * handle;
	handle = curl_easy_init();
	CURLcode result;
	if (handle)
	{
		//curl_easy_setopt(handle, CURLOPT_URL, "https://finance.yahoo.com/quote/AMZN/history");
		
		/*curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_data2);
		curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)&data);
		free(data.memory);
		data.memory = NULL;
		data.size = 0;
		*/
		string urlA = "https://query1.finance.yahoo.com/v7/finance/download/";
		string symbol = stock.GetTicker();
		string urlB = "?period1=";
		string urlC = "&period2=";
		string startTime = getTimeinSeconds("2016-08-01T16:00:00");
		string endTime = getTimeinSeconds("2018-04-30T16:00:00");
		string urlD = "&interval=1d&events=history&crumb=";
		string url = urlA + symbol + urlB + startTime + urlC + endTime + urlD + sCrumb;
		const char * cURL = url.c_str();
		const char * cookies = sCookies.c_str();
		
		curl_easy_setopt(handle, CURLOPT_COOKIE, cookies);   // Only needed for 1st stock
		curl_easy_setopt(handle, CURLOPT_URL, cURL);
		curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 0);
		curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 0);
		//curl_easy_setopt(handle, CURLOPT_COOKIEJAR, "cookies.txt");
		curl_easy_setopt(handle, CURLOPT_COOKIEFILE, "cookies.txt");
		curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_data2);
		curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)&data);
		result = curl_easy_perform(handle);

		if (result != CURLE_OK)
		{

			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(result));
			//return stock_price;
		}

		istringstream sData(data.memory);
		string sValue, sDate;
		double dValue = 0;
		string line;
		map<Date, double> curprice; // sorted automatically according to key

		// stack
		stack<Date> dates;
		stack<double> adj; 
		int count = 0;
		getline(sData, line);

		while (getline(sData, line)) {
			//cout << line << endl;
			sDate = line.substr(0, line.find_first_of(','));
			line.erase(line.find_last_of(','));
			sValue = line.substr(line.find_last_of(',') + 1);
			dValue = strtod(sValue.c_str(), NULL);

			tm t{};
			istringstream d(sDate);
			d >> get_time(&t, "%Y-%m-%d");
			int year = t.tm_year+1900;
			int mon = t.tm_mon+1;
			int day = t.tm_mday;
			Date dd(year, mon, day);

			// get 30 dates after
			if (dd > stock.GetReportDate()) { // we want count 30 dates after report date
				dates.push(dd);
				adj.push(dValue);
				count++;
			}
			else { // if before report date, we continue to push until reach report date and then count
				dates.push(dd);
				adj.push(dValue);
			}
			if (count == 30) { // starting at 0, if reach 30, 30 dates after report date
				break;
			}
		}

		// get 60 dates before
		while (!dates.empty()) {
			if (dates.top() < stock.GetReportDate()) { // start to count if reach before report date
				curprice[dates.top()] = adj.top();
				dates.pop();
				adj.pop();
				count--;
			}
			else { // ignore the top 30 dates larger than report date
				curprice[dates.top()] = adj.top();
				dates.pop();
				adj.pop();
			}
			if (count == -30) { // starting from 30, if reach -30, 60 dates before report date
				break;
			}
		}

		// curprice is sorted automatically from the oldest date and latest
		stock.SetPrice(curprice);
		stock.SetStartDate((curprice.begin())->first); // oldest date
		stock.SetEndDate((--curprice.end())->first); // latest date
		std::map<Date, double> ret;
		for (auto i = curprice.begin(); i != --curprice.end(); i++) {
			auto j = i;
			j++; // j = i+1
			ret[j->first] = (j->second - i->second) / i->second;
		}
		stock.SetReturn(ret);
		free(data.memory);
		data.size = 0;
		cout << stock.GetTicker();
	}
	else {
		fprintf(stderr, "Curl init failed!\n");
	}

	curl_easy_cleanup(handle);
	//barrier.unlock();
}

void MulTimeForMagic(map<string, Stock>& all_stocks, Share& spy) {
	vector<thread> mythread;
	string sCrumb;
	string sCookies;
	cout << "Note: If any bug shows up or the thread stucks, run again or restart VS. You would be informed if download completed."<< endl;
	curl_global_init(CURL_GLOBAL_DEFAULT);
	// get spy and sCrumb of spy for each thread
	MulGetspyAlsoGetCrumb(spy, sCrumb, sCookies);
	// download each stock in a different thread
	for (map<string, Stock>::iterator itr = all_stocks.begin(); itr != all_stocks.end(); itr++)
		// itr->second is Stock with info stored
		mythread.push_back(thread(Muldownload, ref(itr->second), sCrumb, sCookies));
	for (auto &t : mythread)
		t.join();
	curl_global_cleanup();
	cout << endl<< "Download completed. Total downloaded stocks: " << all_stocks.size() << endl;
}