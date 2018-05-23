#include "Date.h"
#include "Share.h"
#include <iostream>
#include <map>

Share::Share(std::string ticker_, Date s, Date e, std::map<Date, double> price_, std::map<Date, double> ret_)
	:ticker(ticker_), start_date(s), end_date(e), price(price_), ret(ret_) {}

const Date& Share::GetStartDate() const {
	return start_date;
}

const Date& Share::GetEndDate() const {
	return end_date;
}

std::string Share::GetTicker() const {
	return ticker;
}

const std::map<Date, double>& Share::GetPrice() {
	return price;
}

const std::map<Date, double>& Share::GetReturn() {
	return ret;
}


Share Share::slice(Date start, Date end) const {
	std::map<Date, double> priceinterval;
	std::map<Date, double> retinterval;
	// use ++ to keep the end date price
	for (auto i = price.find(start); i != ++(price.find(end)); i++) { //increment first then assignment, just in case.
		priceinterval[i->first] = i->second;
	}
	// since no return on the first day, and need to keep the end date return
	for (auto i = ++ret.find(start); i != ++ret.find(end); i++) {
		retinterval[i->first] = i->second;
	}
	return Share(ticker, start, end, priceinterval, retinterval);
}

/*
const Date& Stock::GetEpsDate() {
	return (*this).eps_date;
}
*/

// constructor by parameters: as a subclass of Share
Stock::Stock(std::string ticker_, Date s, Date e, Date report,
	std::map<Date, double> price_, std::map<Date, double> ret_, double surprise_)
	:Share(ticker_, s, e, price_, ret_), eps_date(report), surprise(surprise_) {}

// copy constructor, assign 5 members to its superclass
Stock::Stock(const Stock& stock) : eps_date(stock.eps_date), surprise(stock.surprise) {
	ticker = stock.ticker;
	start_date = stock.start_date;
	end_date = stock.end_date;
	price = stock.price;
	ret = stock.ret;
}

void Stock::Display(double threshold) {
	std::cout << "Name:" << ticker << std::endl
		<< "Report Date:" << eps_date.GetYear() << "-" << eps_date.GetMonth() << "-" << eps_date.GetDay() << std::endl
		<< "Surprise:" << surprise << std::endl
		<< "price at start: " << price.begin()->second << std::endl << "price at end: " << (--price.end())->second << std::endl
		<< "size: " << price.size() << ", " << ret.size() << std::endl
		<< "StartDate: " << start_date.asString() << std::endl << "EndDate: " << end_date.asString() << std::endl
		<< "Belong to group:";


	if (surprise > threshold)
		std::cout << 1;
	else if (surprise < -threshold)
		std::cout << 3;
	else
		std::cout << 2;
	std::cout << std::endl;
}
