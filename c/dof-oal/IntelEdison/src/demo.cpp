#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <string>

#include <dof/pcr.h>
#include <dof/oal.h>
#include <dof/inet.h>

#include "SparkfunOLED.h"

#define BTN_CNT 2

namespace
{
    static const uint8 BUTTON_MESSAGE_IID_BYTES[] = {0x07, 0x01, 0x00, 0x00, 0x57};
    static const uint8 BUTTON_MESSAGE_I_BYTES[] = {0x02, 0x31, 0x02, 0x20, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00};

    DOFInterfaceID ButtonMessage_ID = nullptr;
    DOFInterface ButtonMessage_Interface = nullptr;

    static uint8 BUTTON_CONFIGURATION_IID_BYTES[] = {0x07, 0x01, 0x00, 0x00, 0x58};
    static uint8 BUTTON_CONFIGURATION_I_BYTES[] = {0x08, 0x31, 0x02, 0x20, 0x03, 0x00, 0x53, 0x01, 0x05, 0x53, 0x01, 0x06, 0x53,
                                                   0x01, 0x07, 0x46, 0x02, 0x31, 0x02, 0x40, 0x03, 0x02, 0x00, 0x01, 0x01, 0x01,
                                                   0x03, 0x01, 0x00, 0x08, 0x02, 0x02, 0x04, 0x03, 0x00, 0x03, 0x02, 0x04, 0x03,
                                                   0x00, 0x04, 0x03, 0x04, 0x03, 0x05, 0x00, 0x05, 0x00, 0x00, 0x06, 0x00, 0x04,
                                                   0x04, 0x03, 0x02, 0x01, 0x07, 0x00, 0x00, 0x08, 0x02, 0x01, 0x00, 0x00, 0x09,
                                                   0x02, 0x00, 0x00, 0x00, 0x01, 0x0A, 0x00};

    DOFInterfaceID ButtonConfiguration_ID = nullptr;
    DOFInterface ButtonConfiguration_Interface = nullptr;


    bool OpenDOFDemo_InitInterfaces()
    {
        ButtonMessage_ID = DOFInterfaceID_Create_Bytes(sizeof(BUTTON_MESSAGE_IID_BYTES), BUTTON_MESSAGE_IID_BYTES);
        if (ButtonMessage_ID)
        {
            ButtonMessage_Interface = DOFInterface_Create(ButtonMessage_ID, sizeof(BUTTON_MESSAGE_I_BYTES), BUTTON_MESSAGE_I_BYTES);
            if(ButtonMessage_Interface)
            {
                ButtonConfiguration_ID = DOFInterfaceID_Create_Bytes(sizeof(BUTTON_CONFIGURATION_IID_BYTES), BUTTON_CONFIGURATION_IID_BYTES);
                if (ButtonConfiguration_ID)
                {
                    ButtonConfiguration_Interface = DOFInterface_Create(ButtonConfiguration_ID, sizeof(BUTTON_CONFIGURATION_I_BYTES), BUTTON_CONFIGURATION_I_BYTES);
                    if(ButtonConfiguration_Interface)
                    {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    void OpenDOFDemo_ShutdownInterfaces()
    {
        DOFInterfaceID_Destroy(ButtonMessage_ID);
        DOFInterface_Destroy(ButtonMessage_Interface);
        DOFInterfaceID_Destroy(ButtonConfiguration_ID);
        DOFInterface_Destroy(ButtonConfiguration_Interface);
    }

    void ButtonMessageInvoke(DOFObjectProvider self, DOFOperation operation, DOFRequest request, DOFInterfaceMethod method, uint16 parameterCount, const DOFValue parameters[]);
    const DOFObjectProvider_t::DOFObjectProviderFns_t ButtonMessage_fns = {NULL, ButtonMessageInvoke, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    const DOFObjectProvider_t ButtonMessageProvider = {&ButtonMessage_fns};

    void ButtonConfigInvoke(DOFObjectProvider self, DOFOperation operation, DOFRequest request, DOFInterfaceMethod method, uint16 parameterCount, const DOFValue parameters[]);
    void ButtonConfigGet(DOFObjectProvider self, DOFOperation operation, DOFRequest request, DOFInterfaceProperty property);
    void ButtonConfigSet(DOFObjectProvider self, DOFOperation operation, DOFRequest request, DOFInterfaceProperty property, DOFValue value);
    const DOFObjectProvider_t::DOFObjectProviderFns_t ButtonConfig_fns = {NULL, ButtonConfigInvoke, ButtonConfigGet, ButtonConfigSet, NULL, NULL, NULL, NULL, NULL, NULL};
    const DOFObjectProvider_t ButtonConfigProvider = {&ButtonConfig_fns};

    enum OpenDOFDemo_ConnectionType
    {
        CONNECTIONTYPE_NOT_CONFIGURED = 0,
        CONNECTIONTYPE_TCP = 1,
        CONNECTIONTYPE_UDP = 3,
        CONNECTIONTYPE_SECURE_GROUP = 4
    };

    // Application State
    DOF dof = nullptr;
    DOFObjectID oid = nullptr;
    DOFCredentials creds = nullptr;
    DOFConnection connection = nullptr;

    DOFSystem secure_system = nullptr;
    DOFObject obj = nullptr;
    DOFObject broadObj = nullptr;

    DOFOperation buttonMessageOp = nullptr;
    DOFOperation buttonConfigOp = nullptr;

    uint8 channel = 0;
    std::string mac;
    std::vector<std::string> messages;
    std::string defaultAddress;
    uint16 defaultPort;

    bool isLocal = false;

    OpenDOFDemo_ConnectionType type = CONNECTIONTYPE_TCP;

    SparkFunOled board;
    SparkfunGamepad gamepad;


    bool OpenDOFDemo_Init()
    {
        DOF_Init();
        InetTransport_Init();

        if( OpenDOFDemo_InitInterfaces() )
        {
            dof = DOF_Create(NULL);
            if (dof)
            {
                return true;
            }
            OpenDOFDemo_ShutdownInterfaces();
        }

        return false;
    }

    void OpenDOFDemo_Shutdown()
    {
        if (connection)
        {
            DOFConnection_Disconnect(connection);
            DOFConnection_Destroy(connection);
        }

        DOFOperation_Destroy(buttonMessageOp);
        DOFOperation_Destroy(buttonConfigOp);

        DOFObject_Destroy(broadObj);
        DOFObject_Destroy(obj);

        DOFSystem_Destroy(secure_system);

        DOFObjectID_Destroy(oid);

        DOFCredentials_Destroy(creds);

        DOF_Destroy(dof);

        OpenDOFDemo_ShutdownInterfaces();

        InetTransport_Shutdown();
        DOF_Shutdown();
    }

    DOFConnection OpenDOFDemo_CreateConnection(DOF dof, OpenDOFDemo_ConnectionType type, DOFCredentials creds, std::string address,
                                               uint16 port)
    {
        DOFConnection ret = NULL;
        DOFAddress addr = InetTransport_CreateAddress(address.c_str(), port);
        if (addr)
        {
            DOFConnectionConfigBuilder ccb;
            if (type == CONNECTIONTYPE_UDP)
            {
                ccb = DOFConnectionConfigBuilder_Create(DOFCONNECTIONTYPE_DATAGRAM, addr);
            }
            else
            {
                ccb = DOFConnectionConfigBuilder_Create(DOFCONNECTIONTYPE_STREAM, addr);
            }

            if (ccb)
            {
                if (DOFConnectionConfigBuilder_SetCredentials(ccb, creds) &&
                    DOFConnectionConfigBuilder_SetSecurityDesire(ccb, DOFSECURITYDESIRE_SECURE))
                {
                    DOFConnectionConfig cc = DOFConnectionConfigBuilder_Build(ccb);
                    if (cc)
                    {
                        ret = DOF_CreateConnection(dof, cc);
                        if (ret)
                        {
                            if (!DOFConnection_Connect(ret, 15000, NULL))
                            {
                                DOFConnection_Destroy(ret);
                                ret = NULL;
                            }
                        }

                        DOFConnectionConfig_Destroy(cc);
                    }
                }
                DOFConnectionConfigBuilder_Destroy(ccb);
            }
            DOFAddress_Destroy(addr);
        }

        return ret;
    }

    DOFSystem OpenDOFDemo_CreateSecureSystem(DOF dof, DOFCredentials creds)
    {
        DOFSystem ret = NULL;
        DOFSystemConfigBuilder scb = DOFSystemConfigBuilder_Create();
        if (scb)
        {
            if (DOFSystemConfigBuilder_SetCredentials(scb, creds))
            {
                DOFSystemConfig sc = DOFSystemConfigBuilder_Build(scb);
                if (sc)
                {
                    ret = DOF_CreateSystem(dof, sc, 15000, NULL);

                    DOFSystemConfig_Destroy(sc);
                }
            }
            DOFSystemConfigBuilder_Destroy(scb);
        }
        return ret;
    }

    boolean OpenDOFDemo_StartDefaultSetup()
    {
        connection = OpenDOFDemo_CreateConnection(dof, CONNECTIONTYPE_TCP, creds, defaultAddress, defaultPort);
        if (connection)
        {
            secure_system = OpenDOFDemo_CreateSecureSystem(dof, creds);
            if (secure_system)
            {
                broadObj = DOFSystem_CreateObject(secure_system, DOFOBJECTID_BROADCAST);
                if(broadObj)
                {
                    obj = DOFSystem_CreateObject(secure_system, oid);
                    if (obj)
                    {
                        buttonMessageOp = DOFObject_BeginProvide(obj, ButtonMessage_Interface, DOF_TIMEOUT_NEVER, (const DOFObjectProvider) &ButtonMessageProvider, nullptr);

                        buttonConfigOp = DOFObject_BeginProvide(obj, ButtonConfiguration_Interface, DOF_TIMEOUT_NEVER, (const DOFObjectProvider) &ButtonConfigProvider, nullptr);
                        return true;
                    }
                }
            }
            board.Display("System Failed");
        }
        board.Display("Connection Failed");

        return false;
    }

    void OpenDOFDemo_Clear()
    {
        board.clear();
    }

    void OpenDOFDemo_ShowMessage()
    {
        OpenDOFDemo_Clear();
        for (uint8 i = 0; i < BTN_CNT; ++i)
        {
            board.Display(std::to_string(i) + ":" + messages[i].substr(0, 8));
        }
    }

    void OpenDOFDemo_ChangeConnection(OpenDOFDemo_ConnectionType ntype, std::string address, uint16 port)
    {
        DOFConnection old = connection;
        connection = OpenDOFDemo_CreateConnection(dof, ntype, creds, address, port);
        if (connection)
        {
            type = ntype;
            if (old)
            {
                DOFConnection_Disconnect(old);
                DOFConnection_Destroy(old);
            }
        }
    }

    void OpenDOFDemo_SetMessage(uint8 btn, const char *message)
    {
        messages[btn - 1] = message;
    }

    void OpenDOFDemo_SendMessage(uint8 btn)
    {
        DOFValue vals[2];
        vals[0] = DOFValueUInt8_Create(channel);
        vals[1] = DOFValueString_Create(3, messages[btn - 1].length(), messages[btn - 1].c_str());

        DOFResult res = DOFObject_Invoke(broadObj, DOFInterface_GetMethod(ButtonMessage_Interface, 0), 2, vals, nullptr, 1000, nullptr);
        DOFResult_Destroy(res);

        DOFValue_Destroy(vals[0]);
        DOFValue_Destroy(vals[1]);
    }

    void OpenDOFDemo_ShowConfig()
    {
        std::string connection_info;
        if(type == CONNECTIONTYPE_TCP)
            connection_info += "TCP ";
        else
            connection_info += "UDP ";

        if(!isLocal)
            connection_info += "Cloud";
        else
            connection_info += "Local";

        OpenDOFDemo_Clear();
        board.Display(mac.substr(0, 9));
        board.Display("  " + mac.substr(9, mac.length()));
        board.Display("Connection");
        board.Display(connection_info);
        board.Display("Channel: " + std::to_string(channel));
    }

    void OpenDOFDemo_CheckButtonState()
    {
        uint8 state = gamepad.GetKeyState();
        if (state & SparkfunGamepad::A)
        {
            OpenDOFDemo_SendMessage(1);
        }

        if (state & SparkfunGamepad::B)
        {
            OpenDOFDemo_SendMessage(2);
        }

        if (state & SparkfunGamepad::SELECT)
        {
            OpenDOFDemo_Clear();
        }
        else if (state & SparkfunGamepad::UP)
        {
            OpenDOFDemo_ShowConfig();
        }
        else if (state & SparkfunGamepad::DOWN)
        {
            OpenDOFDemo_ShowMessage();
        }
    }

    void OpenDOFDemo_Display(const std::string &message)
    {
        board.Display(message);
    }

    void ButtonMessageInvoke(DOFObjectProvider self, DOFOperation operation, DOFRequest request, DOFInterfaceMethod method, uint16 parameterCount, const DOFValue parameters[])
    {
        uint8 chn = DOFValueUInt8_Get(parameters[0]);
        if (chn == channel)
        {
            OpenDOFDemo_Clear();
            OpenDOFDemo_Display("Received:");
            OpenDOFDemo_Display(DOFValueString_Get(parameters[1]));
        }
        DOFRequestInvoke_Return(request, 0, NULL);
    }

    void ButtonConfigInvoke(DOFObjectProvider self, DOFOperation operation, DOFRequest request, DOFInterfaceMethod method, uint16 parameterCount, const DOFValue parameters[])
    {
        DOFItemID meth = DOFInterfaceMethod_GetItemID(method);
        switch (meth)
        {
            case 2:
            {
                // tcpConnectionItem
                const char *address = NULL;
                uint16 port = defaultPort;
                if(parameters[0])
                    address = DOFValueString_Get(parameters[0]);
                else
                    address = defaultAddress.c_str();

                if (parameters[1])
                    port = DOFValueUInt16_Get(parameters[1]);

                DOFRequestInvoke_Return(request, 0, NULL);

                OpenDOFDemo_ChangeConnection(CONNECTIONTYPE_TCP, address, port);
                break;
            }
            case 3:
            {
                // udpConnectionItem
                const char *address = NULL;
                uint16 port = defaultPort;
                if(parameters[0])
                    address = DOFValueString_Get(parameters[0]);
                else
                    address = defaultAddress.c_str();

                if (parameters[1])
                    port = DOFValueUInt16_Get(parameters[1]);

                DOFRequestInvoke_Return(request, 0, NULL);

                OpenDOFDemo_ChangeConnection(CONNECTIONTYPE_UDP, address, port);

                break;
            }
            case 5:
                // displayConfigurationItem
                OpenDOFDemo_ShowConfig();
                DOFRequestInvoke_Return(request, 0, NULL);
                break;
            case 6:
            {
                // getConfigurationItem
                DOFValue out[4];
                out[0] = DOFValueString_Create(3, defaultAddress.length(), defaultAddress.c_str());
                out[1] = NULL;
                out[2] = NULL;
                out[3] = DOFValueUInt8_Create(1);
                DOFRequestInvoke_Return(request, 4, out);

                for (uint8 i = 0; i < 4; ++i)
                {
                    if (out[i])
                        DOFValue_Destroy(out[i]);
                }
            }
                break;
            case 8:
            {
                // setButtonMessageItem
                uint8 btn = DOFValueUInt8_Get(parameters[0]);
                if (btn > 0 && btn <= BTN_CNT)
                {
                    OpenDOFDemo_SetMessage(btn, DOFValueString_Get(parameters[1]));
                    DOFRequestInvoke_Return(request, 0, NULL);
                }
                else
                {
                    DOFException e = DOFProviderException_Create(DOFInterface_GetException(DOFInterfaceMethod_GetInterface(method), 10), 0, NULL);
                    DOFRequest_Throw(request, e);
                    DOFException_Destroy(e);
                }
            }
                break;
            case 4:
                // secureMulticastConnectionItem
                // Fall through intended
            case 7:
                // saveConfigurationItem
                // Fall through intended
            case 9:
                // configureWiFiItem
                // Fall through intended
            default:
                DOFRequest_Throw(request, NULL);
        };
    }

    void ButtonConfigGet(DOFObjectProvider self, DOFOperation operation, DOFRequest request, DOFInterfaceProperty property)
    {
        if (DOFInterfaceProperty_GetItemID(property) == 0) // Button Count
        {
            DOFValue val = DOFValueUInt8_Create(BTN_CNT);
            DOFRequestGet_Return(request, val);
            DOFValue_Destroy(val);
        }
        else if (DOFInterfaceProperty_GetItemID(property) == 1) // Channel
        {
            DOFValue val = DOFValueUInt8_Create(channel);
            DOFRequestGet_Return(request, val);
            DOFValue_Destroy(val);
        }
        else
        {
            DOFRequest_Throw(request, NULL);
        }
    }

    void ButtonConfigSet(DOFObjectProvider self, DOFOperation operation, DOFRequest request, DOFInterfaceProperty property, DOFValue value)
    {
        if (DOFInterfaceProperty_GetItemID(property) == 1) // Channel
        {
            channel = DOFValueUInt8_Get(value);
            DOFRequestSet_Return(request);
        }
        else
        {
            DOFRequest_Throw(request, NULL);
        }
    }

    DOFCredentials DOFCredentials_Create_FromFile(std::string file)
    {
        DOFCredentials ret = NULL;

        // Open file at end
        std::ifstream input{file, std::ios::binary | std::ios::ate};

        // get position
        auto length = input.tellg();
        input.seekg(0, std::ios::beg); // Set file pointer to beginning of file

        if (length > 0)
        {
            // Read data
            char *filedata = new char[length];
            input.read(filedata, length);

            // create credentials from file data
            ret = DOFCredentials_Create_Bytes(length, reinterpret_cast<uint8 *>(filedata));

            delete[](filedata);
        }
        return ret;
    }

    bool OpenDOFDemo_ReadConfigFile(const char *filename)
    {
        std::string credfile;
        std::ifstream input(filename);

        input >> mac >> defaultAddress >> defaultPort;
        input.get(); // Read to get to the next line in the config

        getline(input, credfile);
        creds = DOFCredentials_Create_FromFile(credfile);
        if(creds)
        {
            std::string mess;
            getline(input, mess);
            while (input)
            {
                messages.push_back(mess);
                getline(input, mess);
            }
            input.close();

            std::string mac_id = "[2:{" + mac + "}]";

            oid = DOFObjectID_Create_StandardString(mac_id.c_str());
            if(oid)
            {
                return true;
            }
        }
        return false;
    }
}

int main(int argc, char *argv[])
{
	int ret = 0;

	if(argc > 0)
	{
        if(OpenDOFDemo_Init())
        {
            if(OpenDOFDemo_ReadConfigFile(argv[1]))
            {
                if(OpenDOFDemo_StartDefaultSetup())
                {
                    OpenDOFDemo_ShowConfig();

                    while (true)
                    {
                        OpenDOFDemo_CheckButtonState();
                        PCRThread_Sleep(100);
                    }
                }

                OpenDOFDemo_Shutdown();
            }
        }
	}
	else
	{
		std::cout << "program <configuration file>" << std::endl;
	}

	return ret;
}
