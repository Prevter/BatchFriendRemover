// TODO: Refactor this code to be more readable and maintainable

#include <Geode/Geode.hpp>

using namespace geode::prelude;

#define TAG_CONFIRM_UNFRIEND 0x21
#define TAG_CONFIRM_UNBLOCK 0x22

#include <Geode/modify/FriendsProfilePage.hpp>
#include <Geode/modify/GJUserCell.hpp>

struct UserData {
    int32_t m_accountID;
    CCMenuItemToggler *m_checkbox;

    std::string m_username;
};

static UserListType g_userListType;
static std::vector<UserData> g_userData;
static CCMenuItemToggler *g_selectAll;
static std::function<void(UserListType)> g_batchRemoveAlertHandlerCallback = nullptr;

void updateSelectAllCheckbox() {
    if (!g_selectAll) return;

    bool allSelected = true;
    for (auto &userData: g_userData) {
        bool isCheckboxOn = userData.m_checkbox->isOn();
        if (!isCheckboxOn) {
            allSelected = false;
            break;
        }
    }

    g_selectAll->toggle(allSelected);
}

class BatchRemoveAlertHandler : public FLAlertLayerProtocol {
    void FLAlert_Clicked(FLAlertLayer *modal, bool btn2) override {
        if (btn2) {
            auto tag = modal->getTag();
            if (tag == TAG_CONFIRM_UNFRIEND) {
                // show alert
                if (g_batchRemoveAlertHandlerCallback)
                    g_batchRemoveAlertHandlerCallback(UserListType::Friends);

                g_batchRemoveAlertHandlerCallback = nullptr;
            } else if (tag == TAG_CONFIRM_UNBLOCK) {
                // show alert
                if (g_batchRemoveAlertHandlerCallback)
                    g_batchRemoveAlertHandlerCallback(UserListType::Blocked);

                g_batchRemoveAlertHandlerCallback = nullptr;
            }
        }
    }
};

static BatchRemoveAlertHandler g_batchRemoveAlertHandler;

class $modify(FriendsListExt, FriendsProfilePage) {
    bool init(UserListType type) {
        if (!FriendsProfilePage::init(type)) return false;

        g_userListType = type;

        auto myButton = CCMenuItemSpriteExtra::create(
                CCSprite::createWithSpriteFrameName(
                        type == UserListType::Friends ? "accountBtn_removeFriend_001.png" : "GJ_trashBtn_001.png"),
                this, menu_selector(FriendsListExt::onBatchRemove)
        );
        myButton->setPosition({0, -269.f});

        g_selectAll = CCMenuItemToggler::createWithStandardSprites(this, menu_selector(FriendsListExt::onSelectAll),
                                                                   0.7f);
        g_selectAll->setPosition({40, -258.f});

        auto *mainLayer = getChildOfType<CCLayer>(this, 0);
        auto *menu = getChildOfType<CCMenu>(mainLayer, 0);
        menu->addChild(myButton);
        menu->addChild(g_selectAll);

        // Add "All" label
        auto allLabel = CCLabelBMFont::create("All", "goldFont.fnt");
        allLabel->setScale(0.55f);
        allLabel->setPosition({143.f, 38.f});
        mainLayer->addChild(allLabel);

        return true;
    }

    void getUserListFinished(cocos2d::CCArray *arr, UserListType type) {
        g_userData.clear();
        FriendsProfilePage::getUserListFinished(arr, type);
    }

    void onBatchRemove(CCObject *) {
        // log selected users
        std::vector<UserData> selectedUsers;
        for (auto &userData: g_userData) {
            if (userData.m_checkbox->isOn()) {
                selectedUsers.push_back(userData);
            }
        }

        if (selectedUsers.empty()) {
            FLAlertLayer::create(
                    this, "Nothing here...",
                    fmt::format("You <cr>have not selected</c> any users to <cy>{}</c>.",
                                g_userListType == UserListType::Friends ? "unfriend" : "unblock").c_str(),
                    "OK", nullptr
            )->show();
            return;
        }

        FLAlertLayer *modal;
        if (g_userListType == UserListType::Friends) {
            modal = FLAlertLayer::create(
                    &g_batchRemoveAlertHandler, "Unfriend",
                    fmt::format(
                            "Are you sure you want to <cy>unfriend</c> {} user{}?",
                            selectedUsers.size(), selectedUsers.size() > 1 ? "s" : ""),
                    "Back", "Unfriend"
            );
            modal->setTag(TAG_CONFIRM_UNFRIEND);
            modal->m_button2->updateBGImage("GJ_button_06.png");
        } else {
            modal = FLAlertLayer::create(
                    &g_batchRemoveAlertHandler, "Unblock",
                    fmt::format("Are you sure you want to <cy>unblock</c> {} user{}?",
                                selectedUsers.size(), selectedUsers.size() > 1 ? "s" : ""),
                    "Back", "Unblock"
            );
            modal->setTag(TAG_CONFIRM_UNBLOCK);
            modal->m_button2->updateBGImage("GJ_button_05.png");
        }

        g_batchRemoveAlertHandlerCallback = [this, selectedUsers](UserListType type) {
            for (auto &userData: selectedUsers) {
                if (type == UserListType::Friends) {
                    geode::log::info("Unfriending user: {} [{}]", userData.m_username, userData.m_accountID);
                    // unfriend user
                    // unfriendUser(userData.m_accountID);
                } else {
                    geode::log::info("Unblocking user: {} [{}]", userData.m_username, userData.m_accountID);
                    // unblock user
                    // unblockUser(userData.m_accountID);
                }
            }

            // show alert
            if (type == UserListType::Blocked) {
                FLAlertLayer::create(
                        this, "Unblocked",
                        fmt::format("You have successfully <cy>unblocked</c> {} users.", selectedUsers.size()).c_str(),
                        "OK", nullptr
                )->show();
            } else {
                FLAlertLayer::create(
                        this, "Unfriended",
                        fmt::format("You have successfully <cy>unfriended</c> {} users.", selectedUsers.size()).c_str(),
                        "OK", nullptr
                )->show();
            }

            geode::log::info("Done!");
        };

        modal->show();
    }

    void onSelectAll(CCObject *) {
        this->schedule(schedule_selector(FriendsListExt::onSelectAllScheduled), 0);
    }

    void onSelectAllScheduled(float) {
        this->unschedule(schedule_selector(FriendsListExt::onSelectAllScheduled));
        bool allSelected = g_selectAll->isOn();
        for (auto &userData: g_userData) {
            userData.m_checkbox->toggle(allSelected);
        }
    }
};

class $modify(GJUserCellExt, GJUserCell) {
    int32_t m_accountID;
    CCMenuItemToggler *m_checkbox;

    void loadFromScore(GJUserScore *score) {
        GJUserCell::loadFromScore(score);

        // Save accountID to use in callback later
        m_fields->m_accountID = score->m_accountID;

        // Add checkbox
        auto checkbox = CCMenuItemToggler::createWithStandardSprites(
                this, menu_selector(GJUserCellExt::onCheckbox), 0.65f);
        checkbox->setPosition({-35, -138});

        auto *mainLayer = getChildOfType<CCLayer>(this, 1);
        auto *menu = getChildOfType<CCMenu>(mainLayer, 0);
        menu->addChild(checkbox);

        // Save checkbox for later
        std::string username = score->m_userName;
        g_userData.push_back({score->m_accountID, checkbox, username});
        m_fields->m_checkbox = checkbox;
    }

    void onCheckbox(CCObject *sender) {
        this->schedule(schedule_selector(GJUserCellExt::onCheckboxScheduled), 0);
    }

    void onCheckboxScheduled(float) {
        this->unschedule(schedule_selector(GJUserCellExt::onCheckboxScheduled));
        updateSelectAllCheckbox();
    }
};