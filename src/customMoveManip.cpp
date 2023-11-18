#include <CollisionCandidatesFinder.h>
#include <BulletCollisionHandler.h>
#include <BulletOpenGLWidget.h>

#include <maya/MFnPlugin.h>
#include <maya/MStreamUtils.h>

#include <QtWidgets/qtextedit.h>





class MyTickCallback {
public:
    static void myTickCallback(btDynamicsWorld* world, btScalar timeStep) {
        int numManifolds = world->getDispatcher()->getNumManifolds();
        MString infoMsg = MString("numManifolds = ") + numManifolds;
        MGlobal::displayInfo(infoMsg);

        for (int i = 0; i < numManifolds; ++i) {
            btPersistentManifold* contactManifold = world->getDispatcher()->getManifoldByIndexInternal(i);
            const btCollisionObject* obA = contactManifold->getBody0();
            const btCollisionObject* obB = contactManifold->getBody1();

            int numContacts = contactManifold->getNumContacts();
            for (int j = 0; j < numContacts; ++j) {
                btManifoldPoint& pt = contactManifold->getContactPoint(j);
                if (pt.getDistance() < 0.f) {
                    const btVector3& ptA = pt.getPositionWorldOnA();
                    const btVector3& ptB = pt.getPositionWorldOnB();
                    const btVector3& normalOnB = pt.m_normalWorldOnB;

                    // Get the impulse
                    btScalar appliedImpulse = pt.getAppliedImpulse();

                    // Optionally, you can calculate the power of impact (impulse * velocity)
                    // Note: This is a simplification and may not be accurate for all cases.
                    // btScalar impactPower = appliedImpulse * someVelocityMagnitude;

                    // Handle the collision point, print it to Maya's console
                    unsigned long long userPointer0 = reinterpret_cast<unsigned long long>(obA->getUserPointer());
                    unsigned long long userPointer1 = reinterpret_cast<unsigned long long>(obB->getUserPointer());

                    // Convert appliedImpulse to MString
                    MString impulseString = MString() + appliedImpulse;

                    // Construct the message
                    MString msg = "Collision detected between: ";
                    msg += MString() + userPointer0 + " and " + MString() + userPointer1;
                    msg += ", Applied Impulse: " + impulseString;
                    MGlobal::displayInfo(msg);
                }
            }
        }
    }
};






class CustomMoveManip : public MPxManipContainer
{
public:
    CustomMoveManip();
    ~CustomMoveManip() override;

    static void* creator();
    static MStatus initialize();
    MStatus createChildren() override;
    MStatus connectToDependNode(const MObject&) override;
    // Viewport 2.0 rendering
    void drawUI(MHWRender::MUIDrawManager&, const MHWRender::MFrameContext&) const override;
    MStatus doDrag() override;
    MStatus doPress() override;

    void applyTransformToActiveObjectTransform(MMatrix matrix);
    void serializeWorldToFile();

private:
    void updateManipLocations(const MObject& node);
public:
    MDagPath fFreePointManipDagPath;
    bool created = false;
    static MTypeId id;

    CollisionCandidatesFinder collisionCandidatesFinder;
    BulletCollisionHandler bulletCollisionHandler;
};

MTypeId CustomMoveManip::id(0x8001d);

CustomMoveManip::CustomMoveManip()
{

}

CustomMoveManip::~CustomMoveManip()
{

}

void* CustomMoveManip::creator()
{
    return new CustomMoveManip();
}

MStatus CustomMoveManip::initialize()
{
    MStatus stat;
    stat = MPxManipContainer::initialize();

    MString message_text = "CustomMoveManip initialized";
    MGlobal::displayInfo(message_text);
    return stat;
}

void CustomMoveManip::serializeWorldToFile() {

    btDefaultSerializer* serializer = new btDefaultSerializer();
    this->bulletCollisionHandler.dynamicsWorld->serialize(serializer);

    FILE* file = fopen("C:/Users/golub/Documents/maya_viewport_collision_plugin/bulletWorld.bullet", "wb");
    if (file == nullptr) {
        MGlobal::displayWarning("// Handle error - file opening failed");
    }
    else {
        //FILE* file = fopen("bulletWorld.bullet", "wb");
        //fwrite(serializer->getBufferPointer(), serializer->getCurrentBufferSize(), 1, file);
        //fclose(file);
        //delete serializer;

        MGlobal::displayInfo("Attempting to serialize Bullet world.");

        // Convert pointer to string using stringstream
        std::stringstream ss;
        ss << serializer->getBufferPointer();
        MString bufferPointerStr = ss.str().c_str();
        MString bufferPointerMsg = "Buffer pointer: " + bufferPointerStr;
        MGlobal::displayInfo(bufferPointerMsg);

        // Clear stringstream for reuse
        ss.str("");
        ss.clear();

        // Convert buffer size to string
        ss << serializer->getCurrentBufferSize();
        MString bufferSizeStr = ss.str().c_str();
        MString bufferSizeMsg = "Buffer size: " + bufferSizeStr;
        MGlobal::displayInfo(bufferSizeMsg);


        // Commenting out the actual write for debugging
        fwrite(serializer->getBufferPointer(), serializer->getCurrentBufferSize(), 1, file);
        MGlobal::displayInfo("File 'bulletWorld.bullet' opened successfully.");
        fclose(file);
        MGlobal::displayInfo("File 'bulletWorld.bullet' closed successfully.");


        delete serializer;
        MGlobal::displayInfo("Serializer deleted successfully.");
    }
}

MStatus CustomMoveManip::createChildren()
{
    MStatus stat = MStatus::kSuccess;
    MPoint startPoint(0.0, 0.0, 0.0);
    MVector direction(0.0, 1.0, 0.0);
    this->fFreePointManipDagPath = addFreePointTriadManip("pointManip",
        "freePoint");

    if (this->created == false) {
        MGlobal::displayWarning("CREATING WORLD AND STUFF");
        // The constructor must not call createChildren for user-defined manipulators.
        this->collisionCandidatesFinder.addActiveObject();
        this->collisionCandidatesFinder.getSceneMFnMeshes();
        this->collisionCandidatesFinder.initializeRTree();

        this->bulletCollisionHandler.createDynamicsWorld();
        this->bulletCollisionHandler.dynamicsWorld->setInternalTickCallback(MyTickCallback::myTickCallback);
        this->bulletCollisionHandler.updateActiveObject(this->collisionCandidatesFinder.activeMFnMesh);
        this->bulletCollisionHandler.updateColliders(this->collisionCandidatesFinder.allSceneMFnMeshes);

        this->created = true;

        this->bulletCollisionHandler.updateWorld(10);
        this->serializeWorldToFile();

    }

    return stat;
}

MStatus CustomMoveManip::connectToDependNode(const MObject& node)
{
    MStatus stat;
    //
    // This routine connects the translate plug to the position plug on the freePoint
    // manipulator.
    //
    MFnDependencyNode nodeFn(node);
    MPlug tPlug = nodeFn.findPlug("translate", true, &stat);
    MFnFreePointTriadManip freePointManipFn(this->fFreePointManipDagPath);
    //freePointManipFn.connectToPointPlug(tPlug);
    this->updateManipLocations(node);
    this->finishAddingManips();
    MPxManipContainer::connectToDependNode(node);
    return stat;
}
// Viewport 2.0 manipulator draw overrides

void CustomMoveManip::updateManipLocations(const MObject& node)
//
// Description
//        setTranslation and setRotation to the parent's transformation.
//
{
    MFnFreePointTriadManip manipFn(this->fFreePointManipDagPath);

    MDagPath dagPath;
    MFnDagNode(node).getPath(dagPath);

    MObject transformNode = dagPath.transform();
    MFnTransform fnTransform(transformNode);
    MTransformationMatrix originalTM = fnTransform.transformation();

    double rot[3];
    MTransformationMatrix::RotationOrder rOrder;
    originalTM.getRotation(rot, rOrder);

    manipFn.setRotation(rot, rOrder);
    manipFn.setTranslation(originalTM.getTranslation(MSpace::kTransform), MSpace::kTransform);

    MStatus status;
}

MStatus CustomMoveManip::doPress()
{
    MStatus status = MPxManipContainer::doPress();

    return status;
}

MStatus CustomMoveManip::doDrag() {
    // 
    // 
    // Update the world.
    this->bulletCollisionHandler.updateWorld(5);

    // Check for collision candidates and update colliders.
    /*
    std::vector<MFnMesh*> collisionCandidates = this->collisionCandidatesFinder.checkNearbyObjects();
    if (!collisionCandidates.empty()) {
        this->bulletCollisionHandler.updateColliders(collisionCandidates);
    }*/

    // Read translation from manip.
    MFnManip3D manipFn(this->fFreePointManipDagPath);
    MPoint currentPosition;
    this->getConverterManipValue(0, currentPosition);
    MPoint currentTranslation = manipFn.translation(MSpace::kWorld);

    // Set transform to proxy object in Bullet's coordinate system.

    this->bulletCollisionHandler.setProxyObjectPosition(
        currentPosition.x + currentTranslation.x, 
        currentPosition.z + currentTranslation.z,
        (currentPosition.y + currentTranslation.y)*-1
    );
    /*
    */

    // Update world again for accuracy.
    this->bulletCollisionHandler.updateWorld(20);

    // Read transform from active object.
    MMatrix activeObjectUpdatedMatrix = this->bulletCollisionHandler.getActiveObjectTransformMMatrix();
    this->applyTransformToActiveObjectTransform(activeObjectUpdatedMatrix);

    // Debugging information.
    //int numObjects = this->bulletCollisionHandler.dynamicsWorld->getNumCollisionObjects();
    //MGlobal::displayInfo(MString("---Number of Collision Objects: ") + numObjects);

    return MS::kUnknownParameter;
}


void CustomMoveManip::applyTransformToActiveObjectTransform(MMatrix matrix) {
    // Get the MFnDagNode of the active object
    MFnDagNode& activeDagNode = this->collisionCandidatesFinder.activeTransformMFnDagNode;

    MDagPath dagPath;
    activeDagNode.getPath(dagPath);

    MGlobal::displayInfo("SETTING TRANSFORM TO ACTIVE OBJECT:" + dagPath.fullPathName());
    std::ostringstream oss;
    oss << "MMatrix: \n";
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            oss << matrix[i][j] << " ";
        }
        oss << "\n";
    }

    MGlobal::displayInfo(MString(oss.str().c_str()));
    
    MFnTransform activeTransform(dagPath);
    MStatus status = activeTransform.set(MTransformationMatrix(matrix));
    if (!status) {
        // Handle the error if the transformation could not be set
        MGlobal::displayError("Error setting transformation: " + status.errorString());
    }
}

void CustomMoveManip::drawUI(MHWRender::MUIDrawManager& drawManager, const MHWRender::MFrameContext& frameContext) const
{
    drawManager.beginDrawable();
    drawManager.setColor(MColor(0.0f, 1.0f, 0.1f));
    drawManager.text(MPoint(0, 0), "Stretch Me!", MHWRender::MUIDrawManager::kLeft);
    drawManager.endDrawable();
}

//
// MoveManipContext
//
// This class is a simple context for supporting a move manipulator.
//
class CustomMoveManipContext : public MPxSelectionContext
{
public:
    CustomMoveManipContext();
    void    toolOnSetup(MEvent& event) override;
    void    toolOffCleanup() override;
    // Callback issued when selection list changes
    static void selectionChanged(void* data);
private:
    MCallbackId id1;
};

CustomMoveManipContext::CustomMoveManipContext()
{
    MString str("Plugin move Manipulator");
    setTitleString(str);
}

void CustomMoveManipContext::toolOnSetup(MEvent&)
{
    MString str("Move the object using the manipulator");
    setHelpString(str);
    selectionChanged(this);
    MStatus status;
    id1 = MModelMessage::addCallback(MModelMessage::kActiveListModified,
        selectionChanged,
        this, &status);
    if (!status) {
        MGlobal::displayError("Model addCallback failed");
    }
}

void CustomMoveManipContext::toolOffCleanup()
{
    MStatus status;
    status = MModelMessage::removeCallback(id1);
    if (!status) {
        MGlobal::displayError("Model remove callback failed");
    }

    MPxContext::toolOffCleanup();
}

void CustomMoveManipContext::selectionChanged(void* data)
{
    MStatus stat = MStatus::kSuccess;

    CustomMoveManipContext* ctxPtr = (CustomMoveManipContext*)data;
    ctxPtr->deleteManipulators();
    MSelectionList list;
    stat = MGlobal::getActiveSelectionList(list);
    MItSelectionList iter(list, MFn::kInvalid, &stat);
    if (MS::kSuccess != stat) {
        return;
    }

    MString testText = "Selection changed";
    MGlobal::displayInfo(testText);

    for (; !iter.isDone(); iter.next()) {
        // Make sure the selection list item is a depend node and has the
        // required plugs before manipulating it.
        MObject dependNode;
        iter.getDependNode(dependNode);
        if (dependNode.isNull() || !dependNode.hasFn(MFn::kDependencyNode))
        {
            MGlobal::displayWarning("Object in selection list is not "
                "a depend node.");
            continue;
        }
        MFnDependencyNode dependNodeFn(dependNode);
        MPlug tPlug = dependNodeFn.findPlug("translate", true, &stat);
        if (tPlug.isNull()) {
            MGlobal::displayWarning("Object cannot be manipulated: " +
                dependNodeFn.name());
            continue;
        }

        MString manipName("customMoveManip");
        MObject manipObject;
        CustomMoveManip* manipulator = (CustomMoveManip*)CustomMoveManip::newManipulator(manipName, manipObject);
        if (NULL != manipulator) {
            ctxPtr->addManipulator(manipObject);
            // Connect the manipulator to the object in the selection list.
            if (!manipulator->connectToDependNode(dependNode))
            {
                MGlobal::displayWarning("Error connecting manipulator to"
                    " object: " + dependNodeFn.name());
            }


        }
    }
}

//
// moveManipContext
//
// This is the command that will be used to create instances
// of our context.
//
class CustoMoveManipContext : public MPxContextCommand
{
public:
    CustoMoveManipContext() {};
    MPoint getCurrentPosition();
    MPxContext* makeObj() override;

public:
    static void* creator();
};

MPxContext* CustoMoveManipContext::makeObj()
{
    return new CustomMoveManipContext();
}

void* CustoMoveManipContext::creator()
{
    return new CustoMoveManipContext;
}

//
// The following routines are used to register/unregister
// the context and manipulator
//
MStatus initializePlugin(MObject obj)
{
    MStatus status;
    MFnPlugin plugin(obj, PLUGIN_COMPANY, "1.0", "Andrew Golubev");
    status = plugin.registerContextCommand("customMoveManipContext",
        &CustoMoveManipContext::creator);
    if (!status) {
        MGlobal::displayError("Error registering customMoveManipContext command");
        return status;
    }
    status = plugin.registerNode("customMoveManip", CustomMoveManip::id,
        &CustomMoveManip::creator, &CustomMoveManip::initialize,
        MPxNode::kManipContainer);
    if (!status) {
        MGlobal::displayError("Error registering customMoveManip node");
        return status;
    }
    return status;
}

MStatus uninitializePlugin(MObject obj)
{
    MStatus status;
    MFnPlugin plugin(obj);
    status = plugin.deregisterContextCommand("customMoveManipContext");
    if (!status) {
        MGlobal::displayError("Error deregistering customMoveManipContext command");
        return status;
    }
    status = plugin.deregisterNode(CustomMoveManip::id);
    if (!status) {
        MGlobal::displayError("Error deregistering customMoveManip node");
        return status;
    }
    return status;
}
