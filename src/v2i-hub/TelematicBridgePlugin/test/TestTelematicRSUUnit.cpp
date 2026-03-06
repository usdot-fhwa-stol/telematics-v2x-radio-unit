#define TESTING_ACCESS
#include <gtest/gtest.h>
#include "TelematicRsuUnit.h"
#include <fstream>
#include <memory>

using namespace TelematicBridge;
using namespace std;

namespace TelematicBridge
{
    class TestTelematicRsuUnit : public ::testing::Test
    {
    protected:
        shared_ptr<TelematicRsuUnit> unit;

        void SetUp() override
        {
            unsetenv("RSU_CONFIG_PATH");
        }

        void TearDown() override
        {
            unit.reset();
            remove("/tmp/test_config.json");
        }

        void createFile(const string& path, const string& content)
        {
            ofstream f(path);
            f << content;
            f.close();
        }

        string getValidConfig()
        {
            return R"({
                "unitConfig": {"unitId": "Unit001"},
                "rsuConfigs": [{
                    "action": "add",
                    "event": "startup",
                    "rsu": {"ip": "192.168.1.10", "port": 161},
                    "snmp": {
                        "user": "admin",
                        "privacyProtocol": "AES",
                        "authProtocol": "SHA",
                        "authPassPhrase": "pass",
                        "privacyPassPhrase": "priv",
                        "rsuMibVersion": "4.1",
                        "securityLevel": "authPriv"
                    }
                }],
                "timestamp": 1234567890
            })";
        }

        Json::Value getUpdateMsg()
        {
            Json::Value msg;
            Json::Value unitConfig;
            unitConfig["unitId"] = "Unit001";
            msg["unitConfig"] = unitConfig;
            
            msg["rsuConfigs"][0]["action"] = "add";
            msg["rsuConfigs"][0]["event"] = "update";
            msg["rsuConfigs"][0]["rsu"]["ip"] = "192.168.1.20";
            msg["rsuConfigs"][0]["rsu"]["port"] = 161;
            msg["rsuConfigs"][0]["snmp"]["user"] = "admin";
            msg["rsuConfigs"][0]["snmp"]["privacyProtocol"] = "AES";
            msg["rsuConfigs"][0]["snmp"]["authProtocol"] = "SHA";
            msg["rsuConfigs"][0]["snmp"]["authPassPhrase"] = "pass";
            msg["rsuConfigs"][0]["snmp"]["privacyPassPhrase"] = "priv";
            msg["rsuConfigs"][0]["snmp"]["rsuMibVersion"] = "4.1";
            msg["rsuConfigs"][0]["snmp"]["securityLevel"] = "authPriv";
            msg["timestamp"] = 1234567890;
            return msg;
        }
    };

    TEST_F(TestTelematicRsuUnit, ConstructorNoConfig)
    {
        EXPECT_THROW({
            unit = make_shared<TelematicRsuUnit>();
        }, runtime_error);
    }

    TEST_F(TestTelematicRsuUnit, ConstructorWithValidConfig)
    {
        createFile("/tmp/test_config.json", getValidConfig());
        setenv("RSU_CONFIG_PATH", "/tmp/test_config.json", 1);
        unit = make_shared<TelematicRsuUnit>();
        ASSERT_NE(unit, nullptr);
        unsetenv("RSU_CONFIG_PATH");
    }

    TEST_F(TestTelematicRsuUnit, ConstructorWithInvalidConfig)
    {
        createFile("/tmp/test_config.json", "{bad}");
        setenv("RSU_CONFIG_PATH", "/tmp/test_config.json", 1);
        EXPECT_THROW({
            unit = make_shared<TelematicRsuUnit>();
        }, runtime_error);
        unsetenv("RSU_CONFIG_PATH");
    }

    TEST_F(TestTelematicRsuUnit, UpdateRSUStatusSuccess)
    {
        createFile("/tmp/test_config.json", getValidConfig());
        setenv("RSU_CONFIG_PATH", "/tmp/test_config.json", 1);
        unit = make_shared<TelematicRsuUnit>();
        ASSERT_TRUE(unit->updateRSUStatus(getUpdateMsg()));
        unsetenv("RSU_CONFIG_PATH");

        ASSERT_EQ(unit->getRsuConfigTopic(), "unit.Unit001.register.rsu.config");
    }

    TEST_F(TestTelematicRsuUnit, UpdateRSUStatusFail)
    {
        createFile("/tmp/test_config.json", getValidConfig());
        setenv("RSU_CONFIG_PATH", "/tmp/test_config.json", 1);
        unit = make_shared<TelematicRsuUnit>();
        Json::Value bad;
        bad["timestamp"] = 1;
        ASSERT_FALSE(unit->updateRSUStatus(bad));
        unsetenv("RSU_CONFIG_PATH");
    }

    TEST_F(TestTelematicRsuUnit, ConstructRegistrationString)
    {
        createFile("/tmp/test_config.json", getValidConfig());
        setenv("RSU_CONFIG_PATH", "/tmp/test_config.json", 1);
        unit = make_shared<TelematicRsuUnit>();
        string json = unit->constructRSURegistrationDataString();
        ASSERT_FALSE(json.empty());
        Json::Value root;
        Json::CharReaderBuilder builder;
        istringstream stream(json);
        string errs;
        ASSERT_TRUE(Json::parseFromStream(builder, stream, &root, &errs));
        unsetenv("RSU_CONFIG_PATH");
    }

    TEST_F(TestTelematicRsuUnit, ConstructResponseSuccess)
    {
        createFile("/tmp/test_config.json", getValidConfig());
        setenv("RSU_CONFIG_PATH", "/tmp/test_config.json", 1);
        unit = make_shared<TelematicRsuUnit>();
        string json = unit->constructRSUConfigResponseDataString(true);
        Json::Value root;
        Json::CharReaderBuilder builder;
        istringstream stream(json);
        string errs;
        Json::parseFromStream(builder, stream, &root, &errs);
        ASSERT_EQ(root["status"].asString(), "success");
        unsetenv("RSU_CONFIG_PATH");
    }

    TEST_F(TestTelematicRsuUnit, ConstructResponseFail)
    {
        createFile("/tmp/test_config.json", getValidConfig());
        setenv("RSU_CONFIG_PATH", "/tmp/test_config.json", 1);
        unit = make_shared<TelematicRsuUnit>();
        string json = unit->constructRSUConfigResponseDataString(false);
        Json::Value root;
        Json::CharReaderBuilder builder;
        istringstream stream(json);
        string errs;
        Json::parseFromStream(builder, stream, &root, &errs);
        ASSERT_EQ(root["status"].asString(), "failed");
        unsetenv("RSU_CONFIG_PATH");
    }

    TEST_F(TestTelematicRsuUnit, ThreadSafety)
    {
        createFile("/tmp/test_config.json", getValidConfig());
        setenv("RSU_CONFIG_PATH", "/tmp/test_config.json", 1);
        unit = make_shared<TelematicRsuUnit>();
        vector<thread> threads;
        vector<string> results(5);
        for (int i = 0; i < 5; ++i)
            threads.emplace_back([this, i, &results]() {
                results[i] = unit->constructRSURegistrationDataString();
            });
        for (auto &t : threads)
            t.join();
        for (const auto &r : results)
            ASSERT_FALSE(r.empty());
        unsetenv("RSU_CONFIG_PATH");
    }

    TEST_F(TestTelematicRsuUnit, Destructor)
    {
        createFile("/tmp/test_config.json", getValidConfig());
        setenv("RSU_CONFIG_PATH", "/tmp/test_config.json", 1);
        unit = make_shared<TelematicRsuUnit>();
        unit.reset();
        unsetenv("RSU_CONFIG_PATH");
    }

    TEST_F(TestTelematicRsuUnit, ProcessConfigUpdateSuccess)
    {
        createFile("/tmp/test_config.json", getValidConfig());
        setenv("RSU_CONFIG_PATH", "/tmp/test_config.json", 1);
        unit = make_shared<TelematicRsuUnit>();

        auto [success, response] = unit->processConfigUpdateAndGenerateResponse(getUpdateMsg());

        ASSERT_TRUE(success);
        ASSERT_FALSE(response.empty());

        Json::Value root;
        Json::CharReaderBuilder builder;
        istringstream stream(response);
        string errs;
        Json::parseFromStream(builder, stream, &root, &errs);
        ASSERT_EQ(root["status"].asString(), "success");

        unsetenv("RSU_CONFIG_PATH");
    }

    TEST_F(TestTelematicRsuUnit, ProcessConfigUpdateFailure)
    {
        createFile("/tmp/test_config.json", getValidConfig());
        setenv("RSU_CONFIG_PATH", "/tmp/test_config.json", 1);
        unit = make_shared<TelematicRsuUnit>();

        Json::Value badMsg;
        badMsg["timestamp"] = 1;

        auto [success, response] = unit->processConfigUpdateAndGenerateResponse(badMsg);

        ASSERT_FALSE(success);
        ASSERT_FALSE(response.empty());

        Json::Value root;
        Json::CharReaderBuilder builder;
        istringstream stream(response);
        string errs;
        Json::parseFromStream(builder, stream, &root, &errs);
        ASSERT_EQ(root["status"].asString(), "failed");
        unsetenv("RSU_CONFIG_PATH");
    }

    TEST_F(TestTelematicRsuUnit, Connect_UnreachableServer)
    {
        createFile("/tmp/test_config.json", getValidConfig());
        setenv("RSU_CONFIG_PATH", "/tmp/test_config.json", 1);
        unit = make_shared<TelematicRsuUnit>();

        // Test with unreachable NATS server (non-existent host)
        EXPECT_THROW({
            unit->connect("nats://nonexistent.server:4222");
        }, TelematicBridgeException);

        unsetenv("RSU_CONFIG_PATH");
    }

    TEST_F(TestTelematicRsuUnit, Connect_RetriesOnFailure)
    {
        createFile("/tmp/test_config.json", getValidConfig());
        setenv("RSU_CONFIG_PATH", "/tmp/test_config.json", 1);
        unit = make_shared<TelematicRsuUnit>();

        // This test verifies that connect() attempts multiple retries
        // By timing the execution, we can infer retry attempts occurred
        auto start = std::chrono::steady_clock::now();
        
        EXPECT_THROW({
            unit->connect("nats://127.0.0.1:9998");
        }, TelematicBridgeException);
        
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
        
        // Should take at least a few seconds due to retries (assuming 3+ attempts with 1 sec sleep)
        // This confirms the retry mechanism is working
        EXPECT_GE(duration, 2); // At least 2 seconds for multiple retry attempts

        unsetenv("RSU_CONFIG_PATH");
    }

    /**
     * @brief Test synchronization when no RSUs are removed from registration
     */
    TEST_F(TestTelematicRsuUnit, SynchronizeRemoveNone_AllRsus)
    {
        // Setup initial config with RSU 192.168.1.10
        createFile("/tmp/test_config.json", getValidConfig());
        setenv("RSU_CONFIG_PATH", "/tmp/test_config.json", 1);
        unit = make_shared<TelematicRsuUnit>();

        // Add health status for RSU 192.168.1.10
        RSUHealthStatusMessage status1("192.168.1.10", 161, "operate", "startup");
        unit->updateRsuHealthStatus(status1);

        // Add available topics for RSU
        unit->getDataSelectionTrackerForTesting()->updateRsuAvailableTopics("192.168.1.10", 161, "topic1", "Unit001");

        // Verify RSU is present before update
        auto snapshot = unit->getTruHealthStatusTrackerForTesting()->getSnapshot();
        EXPECT_EQ(snapshot.getRsuHealthStatusCount(), 1);
        auto rsuIps = unit->getDataSelectionTrackerForTesting()->getLatestRSUIpsWithAvailableTopics();
        EXPECT_EQ(rsuIps.size(), 1);

        // Create update message with empty RSU configs (no action is taken)
        Json::Value updateMsg;
        updateMsg["unitConfig"]["unitId"] = "Unit001";
        updateMsg["rsuConfigs"] = Json::arrayValue; // Empty array
        updateMsg["timestamp"] = 1234567890;

        // Process the config update
        auto [success, response] = unit->processConfigUpdateAndGenerateResponse(updateMsg);

        // Verify that no RSU is removed
        snapshot = unit->getTruHealthStatusTrackerForTesting()->getSnapshot();
        EXPECT_EQ(snapshot.getRsuHealthStatusCount(), 1);

        // Verify that no RSUs are removed in available topics
        rsuIps = unit->getDataSelectionTrackerForTesting()->getLatestRSUIpsWithAvailableTopics();
        EXPECT_EQ(rsuIps.size(), 1);

        unsetenv("RSU_CONFIG_PATH");
    }

    /**
     * @brief Test explicit removal of RSU using "remove" action
     * This tests the synchronization when an RSU is explicitly removed via the "remove" action
     */
    TEST_F(TestTelematicRsuUnit, ExplicitRemoveAction_RemovesRsuFromHealthStatusAndAvailableTopics)
    {
        // Setup initial config with RSU 192.168.1.10
        createFile("/tmp/test_config.json", getValidConfig());
        setenv("RSU_CONFIG_PATH", "/tmp/test_config.json", 1);
        unit = make_shared<TelematicRsuUnit>();

        // Add health status for RSU 192.168.1.10
        RSUHealthStatusMessage status1("192.168.1.10", 161, "operate", "startup");
        unit->updateRsuHealthStatus(status1);

        // Add available topics for RSU 192.168.1.10
        unit->getDataSelectionTrackerForTesting()->updateRsuAvailableTopics("192.168.1.10", 161, "topic1", "Unit001");

        // Verify RSU is present before removal
        auto snapshot = unit->getTruHealthStatusTrackerForTesting()->getSnapshot();
        EXPECT_EQ(snapshot.getRsuHealthStatusCount(), 1);
        auto rsuIps = unit->getDataSelectionTrackerForTesting()->getLatestRSUIpsWithAvailableTopics();
        EXPECT_EQ(rsuIps.size(), 1);

        // Create update message with explicit "remove" action for 192.168.1.10
        Json::Value updateMsg;
        updateMsg["unitConfig"]["unitId"] = "Unit001";
        updateMsg["rsuConfigs"][0]["action"] = "remove";
        updateMsg["rsuConfigs"][0]["event"] = "shutdown";
        updateMsg["rsuConfigs"][0]["rsu"]["ip"] = "192.168.1.10";
        updateMsg["rsuConfigs"][0]["rsu"]["port"] = 161;
        updateMsg["timestamp"] = 1234567890;

        // Process the config update
        auto [success, response] = unit->processConfigUpdateAndGenerateResponse(updateMsg);

        // Verify update was successful
        EXPECT_TRUE(success);

        // Verify that RSU 192.168.1.10 was removed from health status
        snapshot = unit->getTruHealthStatusTrackerForTesting()->getSnapshot();
        EXPECT_EQ(snapshot.getRsuHealthStatusCount(), 0);

        // Verify that RSU 192.168.1.10 was removed from available topics
        rsuIps = unit->getDataSelectionTrackerForTesting()->getLatestRSUIpsWithAvailableTopics();
        EXPECT_EQ(rsuIps.size(), 0);

        unsetenv("RSU_CONFIG_PATH");
    }

    /**
     * @brief Test mixed actions: remove one RSU while keeping another
     * Tests synchronization with both "add" and "remove" actions in the same update
     */
    TEST_F(TestTelematicRsuUnit, MixedActions_RemoveOneKeepOne)
    {
        // Setup initial config with RSU 192.168.1.10
        createFile("/tmp/test_config.json", getValidConfig());
        setenv("RSU_CONFIG_PATH", "/tmp/test_config.json", 1);
        unit = make_shared<TelematicRsuUnit>();

        // Add health status for two RSUs
        RSUHealthStatusMessage status1("192.168.1.10", 161, "operate", "startup");
        RSUHealthStatusMessage status2("192.168.1.20", 161, "operate", "startup");
        unit->updateRsuHealthStatus(status1);
        unit->updateRsuHealthStatus(status2);

        // Add available topics for both RSUs
        unit->getDataSelectionTrackerForTesting()->updateRsuAvailableTopics("192.168.1.10", 161, "topic1", "Unit001");
        unit->getDataSelectionTrackerForTesting()->updateRsuAvailableTopics("192.168.1.20", 161, "topic2", "Unit001");

        // Verify both RSUs are present before update
        auto snapshot = unit->getTruHealthStatusTrackerForTesting()->getSnapshot();
        EXPECT_EQ(snapshot.getRsuHealthStatusCount(), 2);
        auto rsuIps = unit->getDataSelectionTrackerForTesting()->getLatestRSUIpsWithAvailableTopics();
        EXPECT_EQ(rsuIps.size(), 2);

        // Create update message: keep 192.168.1.10 (add action) and remove 192.168.1.20 (remove action)
        Json::Value updateMsg;
        updateMsg["unitConfig"]["unitId"] = "Unit001";
        
        // Keep RSU 192.168.1.10
        updateMsg["rsuConfigs"][0]["action"] = "add";
        updateMsg["rsuConfigs"][0]["event"] = "update";
        updateMsg["rsuConfigs"][0]["rsu"]["ip"] = "192.168.1.10";
        updateMsg["rsuConfigs"][0]["rsu"]["port"] = 161;
        updateMsg["rsuConfigs"][0]["snmp"]["user"] = "admin";
        updateMsg["rsuConfigs"][0]["snmp"]["privacyProtocol"] = "AES";
        updateMsg["rsuConfigs"][0]["snmp"]["authProtocol"] = "SHA";
        updateMsg["rsuConfigs"][0]["snmp"]["authPassPhrase"] = "pass";
        updateMsg["rsuConfigs"][0]["snmp"]["privacyPassPhrase"] = "priv";
        updateMsg["rsuConfigs"][0]["snmp"]["rsuMibVersion"] = "4.1";
        updateMsg["rsuConfigs"][0]["snmp"]["securityLevel"] = "authPriv";
        
        // Remove RSU 192.168.1.20
        updateMsg["rsuConfigs"][1]["action"] = "remove";
        updateMsg["rsuConfigs"][1]["event"] = "shutdown";
        updateMsg["rsuConfigs"][1]["rsu"]["ip"] = "192.168.1.20";
        updateMsg["rsuConfigs"][1]["rsu"]["port"] = 161;
        
        updateMsg["timestamp"] = 1234567890;

        // Process the config update
        auto [success, response] = unit->processConfigUpdateAndGenerateResponse(updateMsg);

        // Verify update was successful
        EXPECT_TRUE(success);

        // Verify that only RSU 192.168.1.10 remains in health status
        snapshot = unit->getTruHealthStatusTrackerForTesting()->getSnapshot();
        EXPECT_EQ(snapshot.getRsuHealthStatusCount(), 1);
        const auto& rsuStatuses = snapshot.getRsuHealthStatus();
        EXPECT_EQ(rsuStatuses[0].getIp(), "192.168.1.10");

        // Verify that only RSU 192.168.1.10 remains in available topics
        rsuIps = unit->getDataSelectionTrackerForTesting()->getLatestRSUIpsWithAvailableTopics();
        EXPECT_EQ(rsuIps.size(), 1);
        EXPECT_EQ(rsuIps[0], "192.168.1.10");

        unsetenv("RSU_CONFIG_PATH");
    }

    /**
     * @brief Test removing multiple RSUs with explicit "remove" action
     * Tests synchronization when multiple RSUs are removed via explicit "remove" actions
     */
    TEST_F(TestTelematicRsuUnit, MultipleExplicitRemoves_RemovesAllSpecifiedRsus)
    {
        // Setup initial config with RSU 192.168.1.10
        createFile("/tmp/test_config.json", getValidConfig());
        setenv("RSU_CONFIG_PATH", "/tmp/test_config.json", 1);
        unit = make_shared<TelematicRsuUnit>();

        // Add health status for three RSUs
        RSUHealthStatusMessage status1("192.168.1.10", 161, "operate", "startup");
        RSUHealthStatusMessage status2("192.168.1.20", 161, "operate", "startup");
        RSUHealthStatusMessage status3("192.168.1.30", 161, "operate", "startup");
        unit->updateRsuHealthStatus(status1);
        unit->updateRsuHealthStatus(status2);
        unit->updateRsuHealthStatus(status3);

        // Add available topics for all RSUs
        unit->getDataSelectionTrackerForTesting()->updateRsuAvailableTopics("192.168.1.10", 161, "topic1", "Unit001");
        unit->getDataSelectionTrackerForTesting()->updateRsuAvailableTopics("192.168.1.20", 161, "topic2", "Unit001");
        unit->getDataSelectionTrackerForTesting()->updateRsuAvailableTopics("192.168.1.30", 161, "topic3", "Unit001");

        // Verify all RSUs are present before removal
        auto snapshot = unit->getTruHealthStatusTrackerForTesting()->getSnapshot();
        EXPECT_EQ(snapshot.getRsuHealthStatusCount(), 3);
        auto rsuIps = unit->getDataSelectionTrackerForTesting()->getLatestRSUIpsWithAvailableTopics();
        EXPECT_EQ(rsuIps.size(), 3);

        // Create update message: remove 192.168.1.20 and 192.168.1.30, keep 192.168.1.10
        Json::Value updateMsg;
        updateMsg["unitConfig"]["unitId"] = "Unit001";
        
        // Keep RSU 192.168.1.10
        updateMsg["rsuConfigs"][0]["action"] = "add";
        updateMsg["rsuConfigs"][0]["event"] = "update";
        updateMsg["rsuConfigs"][0]["rsu"]["ip"] = "192.168.1.10";
        updateMsg["rsuConfigs"][0]["rsu"]["port"] = 161;
        updateMsg["rsuConfigs"][0]["snmp"]["user"] = "admin";
        updateMsg["rsuConfigs"][0]["snmp"]["privacyProtocol"] = "AES";
        updateMsg["rsuConfigs"][0]["snmp"]["authProtocol"] = "SHA";
        updateMsg["rsuConfigs"][0]["snmp"]["authPassPhrase"] = "pass";
        updateMsg["rsuConfigs"][0]["snmp"]["privacyPassPhrase"] = "priv";
        updateMsg["rsuConfigs"][0]["snmp"]["rsuMibVersion"] = "4.1";
        updateMsg["rsuConfigs"][0]["snmp"]["securityLevel"] = "authPriv";
        
        // Remove RSU 192.168.1.20
        updateMsg["rsuConfigs"][1]["action"] = "remove";
        updateMsg["rsuConfigs"][1]["event"] = "shutdown";
        updateMsg["rsuConfigs"][1]["rsu"]["ip"] = "192.168.1.20";
        updateMsg["rsuConfigs"][1]["rsu"]["port"] = 161;
        
        // Remove RSU 192.168.1.30
        updateMsg["rsuConfigs"][2]["action"] = "remove";
        updateMsg["rsuConfigs"][2]["event"] = "shutdown";
        updateMsg["rsuConfigs"][2]["rsu"]["ip"] = "192.168.1.30";
        updateMsg["rsuConfigs"][2]["rsu"]["port"] = 161;
        
        updateMsg["timestamp"] = 1234567890;

        // Process the config update
        auto [success, response] = unit->processConfigUpdateAndGenerateResponse(updateMsg);

        // Verify update was successful
        EXPECT_TRUE(success);

        // Verify that only RSU 192.168.1.10 remains in health status
        snapshot = unit->getTruHealthStatusTrackerForTesting()->getSnapshot();
        EXPECT_EQ(snapshot.getRsuHealthStatusCount(), 1);
        const auto& rsuStatuses = snapshot.getRsuHealthStatus();
        EXPECT_EQ(rsuStatuses[0].getIp(), "192.168.1.10");

        // Verify that only RSU 192.168.1.10 remains in available topics
        rsuIps = unit->getDataSelectionTrackerForTesting()->getLatestRSUIpsWithAvailableTopics();
        EXPECT_EQ(rsuIps.size(), 1);
        EXPECT_EQ(rsuIps[0], "192.168.1.10");

        unsetenv("RSU_CONFIG_PATH");
    }

    /**
     * @brief Test that "delete" action works the same as "remove" action
     * The action parser accepts both "remove" and "delete" as removal actions
     */
    TEST_F(TestTelematicRsuUnit, DeleteAction_WorksSameAsRemove)
    {
        // Setup initial config with RSU 192.168.1.10
        createFile("/tmp/test_config.json", getValidConfig());
        setenv("RSU_CONFIG_PATH", "/tmp/test_config.json", 1);
        unit = make_shared<TelematicRsuUnit>();

        // Add health status for RSU 192.168.1.10
        RSUHealthStatusMessage status1("192.168.1.10", 161, "operate", "startup");
        unit->updateRsuHealthStatus(status1);

        // Add available topics for RSU 192.168.1.10
        unit->getDataSelectionTrackerForTesting()->updateRsuAvailableTopics("192.168.1.10", 161, "topic1", "Unit001");

        // Verify RSU is present before deletion
        auto snapshot = unit->getTruHealthStatusTrackerForTesting()->getSnapshot();
        EXPECT_EQ(snapshot.getRsuHealthStatusCount(), 1);
        auto rsuIps = unit->getDataSelectionTrackerForTesting()->getLatestRSUIpsWithAvailableTopics();
        EXPECT_EQ(rsuIps.size(), 1);

        // Create update message with "delete" action (should work same as "remove")
        Json::Value updateMsg;
        updateMsg["unitConfig"]["unitId"] = "Unit001";
        updateMsg["rsuConfigs"][0]["action"] = "delete";
        updateMsg["rsuConfigs"][0]["event"] = "shutdown";
        updateMsg["rsuConfigs"][0]["rsu"]["ip"] = "192.168.1.10";
        updateMsg["rsuConfigs"][0]["rsu"]["port"] = 161;
        updateMsg["timestamp"] = 1234567890;

        // Process the config update
        auto [success, response] = unit->processConfigUpdateAndGenerateResponse(updateMsg);

        // Verify update was successful
        EXPECT_TRUE(success);

        // Verify that RSU 192.168.1.10 was removed from health status
        snapshot = unit->getTruHealthStatusTrackerForTesting()->getSnapshot();
        EXPECT_EQ(snapshot.getRsuHealthStatusCount(), 0);

        // Verify that RSU 192.168.1.10 was removed from available topics
        rsuIps = unit->getDataSelectionTrackerForTesting()->getLatestRSUIpsWithAvailableTopics();
        EXPECT_EQ(rsuIps.size(), 0);

        unsetenv("RSU_CONFIG_PATH");
    }
}