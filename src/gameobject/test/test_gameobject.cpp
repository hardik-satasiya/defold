#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <dlib/hash.h>
#include <dlib/message.h>
#include <dlib/dstrings.h>
#include <dlib/time.h>
#include <dlib/log.h>
#include <resource/resource.h>
#include "../gameobject.h"
#include "../gameobject_private.h"
#include "gameobject/test/test_gameobject_ddf.h"
#include "../proto/gameobject_ddf.h"

using namespace Vectormath::Aos;

class GameObjectTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        dmGameObject::Initialize();
        dmGameObject::RegisterDDFType(TestGameObject::Spawn::m_DDFDescriptor);

        m_UpdateContext.m_DT = 1.0f / 60.0f;

        dmResource::NewFactoryParams params;
        params.m_MaxResources = 16;
        params.m_Flags = RESOURCE_FACTORY_FLAGS_EMPTY;
        m_Factory = dmResource::NewFactory(&params, "build/default/src/gameobject/test");
        m_Register = dmGameObject::NewRegister(0, 0);
        dmGameObject::RegisterResourceTypes(m_Factory, m_Register);
        dmGameObject::RegisterComponentTypes(m_Factory, m_Register);
        m_Collection = dmGameObject::NewCollection(m_Factory, m_Register, 1024);

        // Register dummy physical resource type
        dmResource::FactoryResult e;
        e = dmResource::RegisterType(m_Factory, "pc", this, PhysCreate, PhysDestroy, 0);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        e = dmResource::RegisterType(m_Factory, "a", this, ACreate, ADestroy, 0);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        e = dmResource::RegisterType(m_Factory, "b", this, BCreate, BDestroy, 0);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        e = dmResource::RegisterType(m_Factory, "c", this, CCreate, CDestroy, 0);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);

        uint32_t resource_type;

        e = dmResource::GetTypeFromExtension(m_Factory, "pc", &resource_type);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        dmGameObject::ComponentType pc_type;
        pc_type.m_Name = "pc";
        pc_type.m_ResourceType = resource_type;
        pc_type.m_Context = this;
        pc_type.m_CreateFunction = PhysComponentCreate;
        pc_type.m_InitFunction = PhysComponentInit;
        pc_type.m_DestroyFunction = PhysComponentDestroy;
        pc_type.m_UpdateFunction = PhysComponentsUpdate;
        dmGameObject::Result result = dmGameObject::RegisterComponentType(m_Register, pc_type);
        ASSERT_EQ(dmGameObject::RESULT_OK, result);

        // A has component_user_data
        e = dmResource::GetTypeFromExtension(m_Factory, "a", &resource_type);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        dmGameObject::ComponentType a_type;
        a_type.m_Name = "a";
        a_type.m_ResourceType = resource_type;
        a_type.m_Context = this;
        a_type.m_CreateFunction = AComponentCreate;
        a_type.m_InitFunction = AComponentInit;
        a_type.m_DestroyFunction = AComponentDestroy;
        a_type.m_UpdateFunction = AComponentsUpdate;
        a_type.m_InstanceHasUserData = true;
        result = dmGameObject::RegisterComponentType(m_Register, a_type);
        ASSERT_EQ(dmGameObject::RESULT_OK, result);

        // B has *not* component_user_data
        e = dmResource::GetTypeFromExtension(m_Factory, "b", &resource_type);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        dmGameObject::ComponentType b_type;
        b_type.m_Name = "b";
        b_type.m_ResourceType = resource_type;
        b_type.m_Context = this;
        b_type.m_CreateFunction = BComponentCreate;
        b_type.m_InitFunction = BComponentInit;
        b_type.m_DestroyFunction = BComponentDestroy;
        b_type.m_UpdateFunction = BComponentsUpdate;
        result = dmGameObject::RegisterComponentType(m_Register, b_type);
        ASSERT_EQ(dmGameObject::RESULT_OK, result);

        // C has component_user_data
        e = dmResource::GetTypeFromExtension(m_Factory, "c", &resource_type);
        ASSERT_EQ(dmResource::FACTORY_RESULT_OK, e);
        dmGameObject::ComponentType c_type;
        c_type.m_Name = "c";
        c_type.m_ResourceType = resource_type;
        c_type.m_Context = this;
        c_type.m_CreateFunction = CComponentCreate;
        c_type.m_InitFunction = CComponentInit;
        c_type.m_DestroyFunction = CComponentDestroy;
        c_type.m_UpdateFunction = CComponentsUpdate;
        c_type.m_InstanceHasUserData = true;
        result = dmGameObject::RegisterComponentType(m_Register, c_type);
        ASSERT_EQ(dmGameObject::RESULT_OK, result);

        m_MaxComponentCreateCountMap[TestGameObject::PhysComponent::m_DDFHash] = 1000000;
    }

    virtual void TearDown()
    {
        dmGameObject::DeleteCollection(m_Collection);
        dmResource::DeleteFactory(m_Factory);
        dmGameObject::DeleteRegister(m_Register);
        dmGameObject::Finalize();
    }

    static dmResource::FResourceCreate    PhysCreate;
    static dmResource::FResourceDestroy   PhysDestroy;
    static dmGameObject::ComponentCreate  PhysComponentCreate;
    static dmGameObject::ComponentInit    PhysComponentInit;
    static dmGameObject::ComponentDestroy PhysComponentDestroy;
    static dmGameObject::ComponentsUpdate PhysComponentsUpdate;

    static dmResource::FResourceCreate    ACreate;
    static dmResource::FResourceDestroy   ADestroy;
    static dmGameObject::ComponentCreate  AComponentCreate;
    static dmGameObject::ComponentInit    AComponentInit;
    static dmGameObject::ComponentDestroy AComponentDestroy;
    static dmGameObject::ComponentsUpdate AComponentsUpdate;

    static dmResource::FResourceCreate    BCreate;
    static dmResource::FResourceDestroy   BDestroy;
    static dmGameObject::ComponentCreate  BComponentCreate;
    static dmGameObject::ComponentInit    BComponentInit;
    static dmGameObject::ComponentDestroy BComponentDestroy;
    static dmGameObject::ComponentsUpdate BComponentsUpdate;

    static dmResource::FResourceCreate    CCreate;
    static dmResource::FResourceDestroy   CDestroy;
    static dmGameObject::ComponentCreate  CComponentCreate;
    static dmGameObject::ComponentInit    CComponentInit;
    static dmGameObject::ComponentDestroy CComponentDestroy;
    static dmGameObject::ComponentsUpdate CComponentsUpdate;

public:

    std::map<uint64_t, uint32_t> m_CreateCountMap;
    std::map<uint64_t, uint32_t> m_DestroyCountMap;

    std::map<uint64_t, uint32_t> m_ComponentCreateCountMap;
    std::map<uint64_t, uint32_t> m_ComponentInitCountMap;
    std::map<uint64_t, uint32_t> m_ComponentDestroyCountMap;
    std::map<uint64_t, uint32_t> m_ComponentUpdateCountMap;
    std::map<uint64_t, uint32_t> m_MaxComponentCreateCountMap;

    std::map<uint64_t, int>      m_ComponentUserDataAcc;

    dmGameObject::UpdateContext m_UpdateContext;
    dmGameObject::HRegister m_Register;
    dmGameObject::HCollection m_Collection;
    dmResource::HFactory m_Factory;
};

template <typename T>
dmResource::CreateResult GenericDDFCreate(dmResource::HFactory factory, void* context, const void* buffer, uint32_t buffer_size, dmResource::SResourceDescriptor* resource, const char* filename)
{
    GameObjectTest* game_object_test = (GameObjectTest*) context;
    game_object_test->m_CreateCountMap[T::m_DDFHash]++;

    T* obj;
    dmDDF::Result e = dmDDF::LoadMessage<T>(buffer, buffer_size, &obj);
    if (e == dmDDF::RESULT_OK)
    {
        resource->m_Resource = (void*) obj;
        return dmResource::CREATE_RESULT_OK;
    }
    else
    {
        return dmResource::CREATE_RESULT_UNKNOWN;
    }
}

template <typename T>
dmResource::CreateResult GenericDDFDestory(dmResource::HFactory factory, void* context, dmResource::SResourceDescriptor* resource)
{
    GameObjectTest* game_object_test = (GameObjectTest*) context;
    game_object_test->m_DestroyCountMap[T::m_DDFHash]++;

    dmDDF::FreeMessage((void*) resource->m_Resource);
    return dmResource::CREATE_RESULT_OK;
}

template <typename T, int add_to_user_data>
static dmGameObject::CreateResult GenericComponentCreate(dmGameObject::HCollection collection,
                                                         dmGameObject::HInstance instance,
                                                         void* resource,
                                                         void* world,
                                                         void* context,
                                                         uintptr_t* user_data)
{
    GameObjectTest* game_object_test = (GameObjectTest*) context;

    if (user_data && add_to_user_data != -1)
    {
        *user_data += add_to_user_data;
    }

    if (game_object_test->m_MaxComponentCreateCountMap.find(T::m_DDFHash) != game_object_test->m_MaxComponentCreateCountMap.end())
    {
        if (game_object_test->m_ComponentCreateCountMap[T::m_DDFHash] >= game_object_test->m_MaxComponentCreateCountMap[T::m_DDFHash])
        {
            return dmGameObject::CREATE_RESULT_UNKNOWN_ERROR;
        }
    }

    game_object_test->m_ComponentCreateCountMap[T::m_DDFHash]++;
    return dmGameObject::CREATE_RESULT_OK;
}

template <typename T>
static dmGameObject::CreateResult GenericComponentInit(dmGameObject::HCollection collection,
                                                        dmGameObject::HInstance instance,
                                                        void* context,
                                                        uintptr_t* user_data)
{
    GameObjectTest* game_object_test = (GameObjectTest*) context;
    game_object_test->m_ComponentInitCountMap[T::m_DDFHash]++;
    return dmGameObject::CREATE_RESULT_OK;
}

template <typename T>
static dmGameObject::UpdateResult GenericComponentsUpdate(dmGameObject::HCollection collection,
                                    const dmGameObject::UpdateContext* update_context,
                                    void* world,
                                    void* context)
{
    GameObjectTest* game_object_test = (GameObjectTest*) context;
    game_object_test->m_ComponentUpdateCountMap[T::m_DDFHash]++;
    return dmGameObject::UPDATE_RESULT_OK;
}


template <typename T>
static dmGameObject::CreateResult GenericComponentDestroy(dmGameObject::HCollection collection,
                                                          dmGameObject::HInstance instance,
                                                          void* world,
                                                          void* context,
                                                          uintptr_t* user_data)
{
    GameObjectTest* game_object_test = (GameObjectTest*) context;
    if (user_data)
    {
        game_object_test->m_ComponentUserDataAcc[T::m_DDFHash] += *user_data;
    }

    game_object_test->m_ComponentDestroyCountMap[T::m_DDFHash]++;
    return dmGameObject::CREATE_RESULT_OK;
}

dmResource::FResourceCreate GameObjectTest::PhysCreate              = GenericDDFCreate<TestGameObject::PhysComponent>;
dmResource::FResourceDestroy GameObjectTest::PhysDestroy            = GenericDDFDestory<TestGameObject::PhysComponent>;
dmGameObject::ComponentCreate GameObjectTest::PhysComponentCreate   = GenericComponentCreate<TestGameObject::PhysComponent,-1>;
dmGameObject::ComponentInit GameObjectTest::PhysComponentInit       = GenericComponentInit<TestGameObject::PhysComponent>;
dmGameObject::ComponentDestroy GameObjectTest::PhysComponentDestroy = GenericComponentDestroy<TestGameObject::PhysComponent>;
dmGameObject::ComponentsUpdate GameObjectTest::PhysComponentsUpdate = GenericComponentsUpdate<TestGameObject::PhysComponent>;

dmResource::FResourceCreate GameObjectTest::ACreate              = GenericDDFCreate<TestGameObject::AResource>;
dmResource::FResourceDestroy GameObjectTest::ADestroy            = GenericDDFDestory<TestGameObject::AResource>;
dmGameObject::ComponentCreate GameObjectTest::AComponentCreate   = GenericComponentCreate<TestGameObject::AResource, 1>;
dmGameObject::ComponentInit GameObjectTest::AComponentInit       = GenericComponentInit<TestGameObject::AResource>;
dmGameObject::ComponentDestroy GameObjectTest::AComponentDestroy = GenericComponentDestroy<TestGameObject::AResource>;
dmGameObject::ComponentsUpdate GameObjectTest::AComponentsUpdate = GenericComponentsUpdate<TestGameObject::AResource>;

dmResource::FResourceCreate GameObjectTest::BCreate              = GenericDDFCreate<TestGameObject::BResource>;
dmResource::FResourceDestroy GameObjectTest::BDestroy            = GenericDDFDestory<TestGameObject::BResource>;
dmGameObject::ComponentCreate GameObjectTest::BComponentCreate   = GenericComponentCreate<TestGameObject::BResource, -1>;
dmGameObject::ComponentInit GameObjectTest::BComponentInit       = GenericComponentInit<TestGameObject::BResource>;
dmGameObject::ComponentDestroy GameObjectTest::BComponentDestroy = GenericComponentDestroy<TestGameObject::BResource>;
dmGameObject::ComponentsUpdate GameObjectTest::BComponentsUpdate = GenericComponentsUpdate<TestGameObject::BResource>;

dmResource::FResourceCreate GameObjectTest::CCreate              = GenericDDFCreate<TestGameObject::CResource>;
dmResource::FResourceDestroy GameObjectTest::CDestroy            = GenericDDFDestory<TestGameObject::CResource>;
dmGameObject::ComponentCreate GameObjectTest::CComponentCreate   = GenericComponentCreate<TestGameObject::CResource, 10>;
dmGameObject::ComponentInit GameObjectTest::CComponentInit       = GenericComponentInit<TestGameObject::CResource>;
dmGameObject::ComponentDestroy GameObjectTest::CComponentDestroy = GenericComponentDestroy<TestGameObject::CResource>;
dmGameObject::ComponentsUpdate GameObjectTest::CComponentsUpdate = GenericComponentsUpdate<TestGameObject::CResource>;

TEST_F(GameObjectTest, Test01)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "goproto01.goc");
    ASSERT_NE((void*) 0, (void*) go);
    bool ret;
    ret = dmGameObject::Update(&m_Collection, &m_UpdateContext, 1);
    ASSERT_TRUE(ret);
    ret = dmGameObject::Update(&m_Collection, &m_UpdateContext, 1);
    ASSERT_TRUE(ret);
    ret = dmGameObject::Update(&m_Collection, &m_UpdateContext, 1);
    ASSERT_TRUE(ret);
    dmGameObject::Delete(m_Collection, go);

    ASSERT_EQ((uint32_t) 0, m_CreateCountMap[TestGameObject::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_DestroyCountMap[TestGameObject::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_ComponentCreateCountMap[TestGameObject::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_ComponentDestroyCountMap[TestGameObject::PhysComponent::m_DDFHash]);
}

TEST_F(GameObjectTest, TestUpdate)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "goproto02.goc");
    ASSERT_NE((void*) 0, (void*) go);
    bool ret = dmGameObject::Update(&m_Collection, &m_UpdateContext, 1);
    ASSERT_TRUE(ret);
    ret = dmGameObject::PostUpdate(&m_Collection, 1);
    ASSERT_TRUE(ret);
    ASSERT_EQ((uint32_t) 1, m_ComponentUpdateCountMap[TestGameObject::PhysComponent::m_DDFHash]);

    dmGameObject::Delete(m_Collection, go);
    ret = dmGameObject::PostUpdate(&m_Collection, 1);
    ASSERT_TRUE(ret);
    ASSERT_EQ((uint32_t) 1, m_CreateCountMap[TestGameObject::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_DestroyCountMap[TestGameObject::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_ComponentCreateCountMap[TestGameObject::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_ComponentDestroyCountMap[TestGameObject::PhysComponent::m_DDFHash]);
}

TEST_F(GameObjectTest, TestPostDeleteUpdate)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "goproto02.goc");
    ASSERT_NE((void*) 0, (void*) go);

    uint32_t message_id = dmHashString32("test");

    dmGameObject::InstanceMessageData data;
    data.m_MessageId = message_id;
    data.m_Component = 0xff;
    data.m_Instance = go;
    data.m_DDFDescriptor = 0x0;
    dmMessage::Post(dmGameObject::GetReplyMessageSocketId(m_Register), message_id, (void*)&data, sizeof(dmGameObject::InstanceMessageData));

    dmGameObject::Delete(m_Collection, go);

    bool ret = dmGameObject::Update(&m_Collection, &m_UpdateContext, 1);
    ASSERT_TRUE(ret);
}

TEST_F(GameObjectTest, TestNonexistingComponent)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "goproto03.goc");
    ASSERT_EQ((void*) 0, (void*) go);
    ASSERT_EQ((uint32_t) 0, m_CreateCountMap[TestGameObject::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_DestroyCountMap[TestGameObject::PhysComponent::m_DDFHash]);

    ASSERT_EQ((uint32_t) 0, m_ComponentCreateCountMap[TestGameObject::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_ComponentDestroyCountMap[TestGameObject::PhysComponent::m_DDFHash]);
}

TEST_F(GameObjectTest, TestPartialNonexistingComponent1)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "goproto04.goc");
    ASSERT_EQ((void*) 0, (void*) go);

    // First one exists
    ASSERT_EQ((uint32_t) 1, m_CreateCountMap[TestGameObject::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_DestroyCountMap[TestGameObject::PhysComponent::m_DDFHash]);
    // Even though the first physcomponent exits the prototype creation should fail before creating components
    ASSERT_EQ((uint32_t) 0, m_ComponentCreateCountMap[TestGameObject::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 0, m_ComponentDestroyCountMap[TestGameObject::PhysComponent::m_DDFHash]);
}

TEST_F(GameObjectTest, TestPartialFailingComponent)
{
    // Only succeed creating the first component
    m_MaxComponentCreateCountMap[TestGameObject::PhysComponent::m_DDFHash] = 1;
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "goproto05.goc");
    ASSERT_EQ((void*) 0, (void*) go);

    ASSERT_EQ((uint32_t) 1, m_CreateCountMap[TestGameObject::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_DestroyCountMap[TestGameObject::PhysComponent::m_DDFHash]);

    // One component should get created
    ASSERT_EQ((uint32_t) 1, m_ComponentCreateCountMap[TestGameObject::PhysComponent::m_DDFHash]);
    ASSERT_EQ((uint32_t) 1, m_ComponentDestroyCountMap[TestGameObject::PhysComponent::m_DDFHash]);
}

TEST_F(GameObjectTest, TestComponentUserdata)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "goproto06.goc");
    ASSERT_NE((void*) 0, (void*) go);

    dmGameObject::Delete(m_Collection, go);
    bool ret = dmGameObject::PostUpdate(&m_Collection, 1);
    ASSERT_TRUE(ret);
    // Two a:s
    ASSERT_EQ(2, m_ComponentUserDataAcc[TestGameObject::AResource::m_DDFHash]);
    // Zero c:s
    ASSERT_EQ(0, m_ComponentUserDataAcc[TestGameObject::BResource::m_DDFHash]);
    // Three c:s
    ASSERT_EQ(30, m_ComponentUserDataAcc[TestGameObject::CResource::m_DDFHash]);
}

TEST_F(GameObjectTest, Null)
{
    dmGameObject::HInstance go = dmGameObject::New(m_Collection, "null.goc");
    ASSERT_NE((void*) 0, (void*) go);

    ASSERT_TRUE(dmGameObject::Init(m_Collection));

    ASSERT_EQ(dmGameObject::RESULT_OK, dmGameObject::PostNamedMessageTo(go, 0, dmHashString32("test"), 0x0, 0));

    dmGameObject::AcquireInputFocus(m_Collection, go);

    dmGameObject::InputAction action;
    action.m_ActionId = dmHashString32("test_action");
    action.m_Value = 1.0f;
    action.m_Pressed = 1;
    action.m_Released = 0;
    action.m_Repeated = 1;

    ASSERT_EQ(dmGameObject::UPDATE_RESULT_OK, dmGameObject::DispatchInput(&m_Collection, 1, &action, 1));

    ASSERT_TRUE(dmGameObject::Update(&m_Collection, 0, 1));

    dmGameObject::Delete(m_Collection, go);
}

TEST_F(GameObjectTest, TestIsVisible)
{
    dmGameObject::UpdateContext m_UpdateContext;
    m_UpdateContext.m_DT = 1.0f / 60.0f;
    m_UpdateContext.m_ViewProj = Vectormath::Aos::Matrix4::identity();
    dmGameObject::HInstance is_visible = dmGameObject::New(m_Collection, "is_visible.goc");
    ASSERT_NE((void*)0, (void*)is_visible);
    ASSERT_TRUE(dmGameObject::Update(&m_Collection, &m_UpdateContext, 1));
    ASSERT_TRUE(dmGameObject::PostUpdate(&m_Collection, 1));
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
