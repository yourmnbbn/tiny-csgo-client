#ifndef __TINY_CSGO_CLIENT_ARGPARSER_HPP__
#define __TINY_CSGO_CLIENT_ARGPARSER_HPP__

#ifdef _WIN32
#pragma once
#endif

#include <string>
#include <vector>
#include <optional>
#include <stdexcept>

// Simple commandline argument parser for tiny csgo client, with strict option type check

enum class OptionAttr : uint8_t
{
	OptionalWithValue,
	OptionalWithoutValue,
	RequiredWithValue,
	RequiredWithoutValue
};

enum class OptionValueType : uint8_t
{
	INT8,
	INT8U,
	INT16,
	INT16U,
	INT32,
	INT32U,
	INT64,
	INT64U,
	STRING
};

class ArgParser
{
	struct Option_t
	{
		Option_t(const char* opt, const char* desc, OptionAttr attr, OptionValueType valtype, const char* defaultstr, uint64_t defaultint) :
			name(opt), 
			description(desc), 
			default_value(defaultstr),
			default_value_int(defaultint),
			attribute(attr), 
			value_type(valtype), 
			exist(false)
		{
		}

		std::string			name;				//Option name
		std::string			description;		//Option description
		std::string			value;				//Option value
		std::string			default_value;		//Option string default value if it requires value
		uint64_t			default_value_int;	//Option int default value if it requires value
		uint64_t			int_value;			//Option int value if it's not string
		OptionAttr			attribute;			//Option attribute
		OptionValueType		value_type;			//Option value type
		bool				exist;				//Is this option exist in the argument vec?
	};

public:

	//We don't have duplication check, so don't add duplicated options.
	void AddOption(const char* option, const char* descriptionOptionAttr, OptionAttr optionAttr, OptionValueType type, const char* defaultstr = "", uint64_t defaultint = 0)
	{
		m_Options.push_back({ option, descriptionOptionAttr, optionAttr, type, defaultstr, defaultint });
	}

	void ParseArgument(int argc, char** argv)
	{
		if (m_Options.empty())
		{
			throw std::logic_error("Please add options before parsing them");
		}

		if (argc <= 1)
		{
			PrintOptions();
			throw std::logic_error("Please add the required parameter to run the program!");
		}

		//Parse argument
		for (int i = 1; i < argc; ++i)
		{
			auto opt = FindOption(argv[i]);
			if (!opt)
			{
				continue;
			}

			auto& option = opt.value().get();

			option.exist = true;
			bool getValue = option.attribute == OptionAttr::OptionalWithValue || option.attribute == OptionAttr::RequiredWithValue;
			if (getValue)
			{
				//There is no further arg to read, but our option requires a value
				if (i == argc - 1)
				{
					snprintf(m_ErrorMsg, sizeof(m_ErrorMsg), "Argument %s need a value but found nothing!", option.name.c_str());
					throw std::logic_error(m_ErrorMsg);
				}

				option.value = argv[++i];
			}
		}

		for (Option_t& opt : m_Options)
		{
			//Check if we have all the required options
			bool required = opt.attribute == OptionAttr::RequiredWithoutValue || opt.attribute == OptionAttr::RequiredWithValue;
			if (required && !opt.exist)
			{
				PrintOptions();
				snprintf(m_ErrorMsg, sizeof(m_ErrorMsg), "Option \"%s\" is required but not found!", opt.name.c_str());
				throw std::logic_error(m_ErrorMsg);
			}

			//Check option value type
			bool requireValue = opt.attribute == OptionAttr::OptionalWithValue || opt.attribute == OptionAttr::RequiredWithValue;
			if (!requireValue)
				continue;

			//We don't check type for strings
			if (opt.value_type == OptionValueType::STRING)
				continue;

			uint64_t value;
			std::string& strValue = opt.exist ? opt.value : opt.default_value;

			//For now we don't check for uint64_t and int64_t, check the rest of all
			if (strValue.starts_with("0x") || strValue.starts_with("0X"))
			{
				for (char& ch : strValue)
					if (ch >= 'A' && ch <= 'Z')
						ch = std::tolower(ch);

				sscanf(strValue.c_str(), "0x%llx", &value);
			}
			else
			{
				if (opt.value_type == OptionValueType::INT8U
					|| opt.value_type == OptionValueType::INT16U
					|| opt.value_type == OptionValueType::INT32U
					|| opt.value_type == OptionValueType::INT64U)
				{
					if (strValue.c_str()[0] == '-')
					{
						snprintf(m_ErrorMsg, sizeof(m_ErrorMsg), 
							"Option %s value is required to be unsigned but found signed value", opt.name.c_str());
						throw std::logic_error(m_ErrorMsg);
					}
				}

				sscanf(strValue.c_str(), "%lld", &value);
			}

			opt.int_value = value;
			switch (opt.value_type)
			{
			case OptionValueType::INT8:
			case OptionValueType::INT8U:
				if (value > 0xFF)
					goto Error;
				break;
			case OptionValueType::INT16:
			case OptionValueType::INT16U:
				if (value > 0xFFFF)
					goto Error;
				break;
			case OptionValueType::INT32:
			case OptionValueType::INT32U:
				if (value > 0xFFFFFFFF)
					goto Error;
				break;
			}

			continue;

		Error:
			snprintf(m_ErrorMsg, sizeof(m_ErrorMsg), "Option %s value overflow!", opt.name.c_str());
			throw std::logic_error(m_ErrorMsg);
		}
	}

	void PrintOptions()
	{
		printf("Options\t\tRequired\tHas value\tDescription\n");
		for (Option_t& opt : m_Options)
			PrintOption(opt);
	}

	bool HasOption(const char* optionName)
	{
		auto& opt = EnsureOptionExist(optionName);
		return opt.exist;
	}

	const char* GetOptionValueString(const char* optionName)
	{
		auto& opt = EnsureOptionExist(optionName);
		return opt.value.c_str();
	}

	uint8_t GetOptionValueInt8U(const char* optionName)
	{
		auto& opt = EnsureOptionExist(optionName);
		return static_cast<uint8_t>(opt.int_value);
	}

	int8_t GetOptionValueInt8(const char* optionName)
	{
		auto& opt = EnsureOptionExist(optionName);
		return static_cast<int8_t>(opt.int_value);
	}

	int16_t GetOptionValueInt16(const char* optionName)
	{
		auto& opt = EnsureOptionExist(optionName);
		return static_cast<int16_t>(opt.int_value);
	}

	uint16_t GetOptionValueInt16U(const char* optionName)
	{
		auto& opt = EnsureOptionExist(optionName);
		return static_cast<uint16_t>(opt.int_value);
	}

	int32_t GetOptionValueInt32(const char* optionName)
	{
		auto& opt = EnsureOptionExist(optionName);
		return static_cast<int32_t>(opt.int_value);
	}

	uint32_t GetOptionValueInt32U(const char* optionName)
	{
		auto& opt = EnsureOptionExist(optionName);
		return static_cast<uint32_t>(opt.int_value);
	}

	int64_t GetOptionValueInt64(const char* optionName)
	{
		auto& opt = EnsureOptionExist(optionName);
		return static_cast<int64_t>(opt.int_value);
	}

	uint64_t GetOptionValueInt64U(const char* optionName)
	{
		auto& opt = EnsureOptionExist(optionName);
		return static_cast<uint64_t>(opt.int_value);
	}

private:
	Option_t& EnsureOptionExist(const char* name)
	{
		auto opt = FindOption(name);
		if (!opt)
		{
			snprintf(m_ErrorMsg, sizeof(m_ErrorMsg), "Can't find option %s, it's not registered", name);
			throw std::logic_error(m_ErrorMsg);
		}

		return opt.value().get();
	}

	std::optional<std::reference_wrapper<Option_t>>	FindOption(const char* option)
	{
		for (Option_t& opt : m_Options)
		{
			if (opt.name == option)
				return opt;
		}

		return {};
	}

	void PrintOption(Option_t& opt)
	{
		bool required = opt.attribute == OptionAttr::RequiredWithoutValue || opt.attribute == OptionAttr::RequiredWithValue;
		bool hasValue = opt.attribute == OptionAttr::OptionalWithValue || opt.attribute == OptionAttr::RequiredWithValue;
		printf("%s\t\t%s\t\t%s\t\t%s\n", 
			opt.name.c_str(), 
			required ? "true" : "false",
			hasValue ? "true" : "false",
			opt.description.c_str());
	}
private:
	char						m_ErrorMsg[1024];
	std::vector<Option_t>		m_Options;
};

#endif // !__TINY_CSGO_CLIENT_ARGPARSER_HPP__
