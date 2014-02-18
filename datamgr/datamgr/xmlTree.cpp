#include <Windows.h>
#include <string>
#include <iostream>
#include <tinystr.h>
#include <tinyxml.h>

std::string GetAppPath()
{//��ȡӦ�ó����Ŀ¼
	char modulePath[MAX_PATH];
	GetModuleFileNameA(NULL, modulePath, MAX_PATH);
	strrchr(modulePath, ('\\'))[1] = 0;
	return modulePath;
}


bool CreateXmlFile(std::string& szFileName)
{//����xml�ļ�,szFilePathΪ�ļ������·��,�������ɹ�����true,����false
	try
	{
		//����һ��XML���ĵ�����
		TiXmlDocument *myDocument = new TiXmlDocument();
		//����һ����Ԫ�ز����ӡ�
		TiXmlElement *RootElement = new TiXmlElement("Persons");
		myDocument->LinkEndChild(RootElement);
		//����һ��PersonԪ�ز����ӡ�
		TiXmlElement *PersonElement = new TiXmlElement("Person");
		RootElement->LinkEndChild(PersonElement);
		//����PersonԪ�ص����ԡ�
		PersonElement->SetAttribute("ID", "1");
		//����nameԪ�ء�ageԪ�ز����ӡ�
		TiXmlElement *NameElement = new TiXmlElement("name");
		TiXmlElement *AgeElement = new TiXmlElement("age");
		PersonElement->LinkEndChild(NameElement);
		PersonElement->LinkEndChild(AgeElement);
		//����nameԪ�غ�ageԪ�ص����ݲ����ӡ�
		TiXmlText *NameContent = new TiXmlText("������");
		TiXmlText *AgeContent = new TiXmlText("22");
		NameElement->LinkEndChild(NameContent);
		AgeElement->LinkEndChild(AgeContent);
		std::string fullPath = GetAppPath();
		fullPath += "\\";
		fullPath += szFileName;
		myDocument->SaveFile(fullPath.c_str());//���浽�ļ�
	}
	catch (std::string& e)
	{
		return false;
	}
	return true;
}

bool ReadXmlFile(std::string& szFileName)
{//��ȡXml�ļ���������
	try
	{
		std::string fullPath = GetAppPath();
		fullPath += "\\";
		fullPath += szFileName;
		//����һ��XML���ĵ�����
		TiXmlDocument *myDocument = new TiXmlDocument(fullPath.c_str());
		myDocument->LoadFile();
		//��ø�Ԫ�أ���Persons��
		TiXmlElement *RootElement = myDocument->RootElement();
		//�����Ԫ�����ƣ������Persons��
		std::cout << RootElement->Value() << std::endl;
		//��õ�һ��Person�ڵ㡣
		TiXmlElement *FirstPerson = RootElement->FirstChildElement();
		//��õ�һ��Person��name�ڵ��age�ڵ��ID���ԡ�
		TiXmlElement *NameElement = FirstPerson->FirstChildElement();
		TiXmlElement *AgeElement = NameElement->NextSiblingElement();
		TiXmlAttribute *IDAttribute = FirstPerson->FirstAttribute();
		//�����һ��Person��name���ݣ��������ǣ�age���ݣ�����ID���ԣ�����
		std::cout << NameElement->FirstChild()->Value() << std::endl;
		std::cout << AgeElement->FirstChild()->Value() << std::endl;
		std::cout << IDAttribute->Value()<< std::endl;
	}
	catch (std::string& e)
	{
		return false;
	}
	return true;
}