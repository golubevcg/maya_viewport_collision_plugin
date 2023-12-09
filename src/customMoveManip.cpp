#include <CustomMoveManip.h>
#include <MayaIncludes.h>
#include <LinearMath/btScalar.h>

MTypeId CustomMoveManip::id(0x8001d);

CustomMoveManip::CustomMoveManip():
        bulletCollisionHandler(BulletCollisionHandler::getInstance()),
        collisionCandidatesFinder(CollisionCandidatesFinder::getInstance()) {
}

CustomMoveManip::~CustomMoveManip() {
    MGlobal::displayInfo("out of destructor!!!!");
}

void* CustomMoveManip::creator() {
    return new CustomMoveManip();
}

MStatus CustomMoveManip::initialize() {
    MStatus stat;
    stat = MPxManipContainer::initialize();
    MGlobal::displayInfo("CustomMoveManip initialized");
    return stat;
}

MStatus CustomMoveManip::createChildren() {
    MStatus stat = MStatus::kSuccess;
    MPoint startPoint(0.0, 0.0, 0.0);
    MVector direction(0.0, 1.0, 0.0);
    this->fFreePointManipDagPath = addFreePointTriadManip("pointManip",
        "freePoint");

    return stat;
}

MStatus CustomMoveManip::connectToDependNode(const MObject& node) {
    MStatus stat;
    //
    // This routine connects the translate plug to the position plug on the freePoint
    // manipulator.
    //
    MGlobal::displayWarning("---CUSTOMMOVEMANIP connectToDependNode");
    MFnDependencyNode nodeFn(node);
    MPlug tPlug = nodeFn.findPlug("translate", true, &stat);
    MFnFreePointTriadManip freePointManipFn(this->fFreePointManipDagPath);
    this->updateManipLocations(node);
    this->finishAddingManips();
    MPxManipContainer::connectToDependNode(node);
    return stat;
}

void CustomMoveManip::updateManipLocations(const MObject& node) {
//
// Description
//        setTranslation and setRotation to the parent's transformation.
//
    MGlobal::displayWarning("---CUSTOMMOVEMANIP updateManipLocations");
    MFnFreePointTriadManip manipFn(this->fFreePointManipDagPath);

    MDagPath dagPath;
    MFnDagNode(node).getPath(dagPath);

    MObject transformNode = dagPath.transform();
    MFnTransform fnTransform(transformNode);
    MTransformationMatrix originalTM = fnTransform.transformation();

    double rot[3];
    MTransformationMatrix::RotationOrder rOrder;
    originalTM.getRotation(rot, rOrder);

    //manipFn.setRotation(rot, rOrder);
    manipFn.setTranslation(originalTM.getTranslation(MSpace::kWorld), MSpace::kWorld);

    MStatus status;
}

MStatus CustomMoveManip::doPress() {
    MStatus status = MPxManipContainer::doPress();
    return status;
}

MStatus CustomMoveManip::doDrag() {
    MGlobal::displayWarning("---CUSTOMMOVEMANIP doDrag");

    // Update the world.

    this->bulletCollisionHandler.updateWorld(5.0);

    // Read translation from manip.
    MFnManip3D manipFn(this->fFreePointManipDagPath);
    MPoint currentPosition;

    this->getConverterManipValue(0, currentPosition);
    MPoint currentTranslation = manipFn.translation(MSpace::kWorld);

    // Get the current position of the rigid body
    btVector3 currentPos = this->bulletCollisionHandler.activeRigidBody->getWorldTransform().getOrigin();
    btVector3 targetPos(
        currentPosition.x + currentTranslation.x,
        currentPosition.z + currentTranslation.z,
        -(currentPosition.y + currentTranslation.y)
    );

    float timeStep = 1.0f / 60.0f;
    btVector3 requiredVelocity = (targetPos - currentPos) / timeStep;

    requiredVelocity *= 0.01;

    // Define a threshold for negligible values
    float threshold = 0.01f;

    // Set small values to zero
    if (std::abs(requiredVelocity.x()) < threshold) requiredVelocity.setX(0);
    if (std::abs(requiredVelocity.y()) < threshold) requiredVelocity.setY(0);
    if (std::abs(requiredVelocity.z()) < threshold) requiredVelocity.setZ(0);

    // Define range for clamping
    float minValue = -0.2f;
    float maxValue = 0.2f;

    // Clamp values within the range
    requiredVelocity.setX(std::min(std::max(requiredVelocity.x(), minValue), maxValue));
    requiredVelocity.setY(std::min(std::max(requiredVelocity.y(), minValue), maxValue));
    requiredVelocity.setZ(std::min(std::max(requiredVelocity.z(), minValue), maxValue));

    // Print the modified velocity
    MString msg = "btVector3: (" + MString() + requiredVelocity.x() + ", "
        + MString() + requiredVelocity.y() + ", "
        + MString() + requiredVelocity.z() + ")";
    MGlobal::displayInfo(msg);

    this->bulletCollisionHandler.activeRigidBody->setLinearVelocity(requiredVelocity);
    this->bulletCollisionHandler.updateWorld(50);;

    // Read transform from active object.
    MMatrix activeObjectUpdatedMatrix = this->bulletCollisionHandler.getActiveObjectTransformMMatrix();
    this->applyTransformAndRotateToActiveObjectTransform(activeObjectUpdatedMatrix);
    
    return MS::kUnknownParameter;
}


void CustomMoveManip::applyTransformAndRotateToActiveObjectTransform(MMatrix matrix) {
    MFnDagNode& activeDagNode = this->collisionCandidatesFinder.activeTransformMFnDagNode;

    MDagPath dagPath;
    activeDagNode.getPath(dagPath);

    MFnTransform activeTransform(dagPath);

    // Create an MTransformationMatrix from the input MMatrix
    MTransformationMatrix transMatrix(matrix);

    // Extract the translation from the MTransformationMatrix
    MVector translation = transMatrix.getTranslation(MSpace::kTransform);

    // Extract the rotation as a quaternion
    MQuaternion rotation;
    transMatrix.getRotationQuaternion(rotation.x, rotation.y, rotation.z, rotation.w);

    // Apply only the translation and rotation to the active transform
    MStatus status = activeTransform.setTranslation(translation, MSpace::kTransform);
    if (!status) {
        MGlobal::displayError("Error setting translation: " + status.errorString());
        return;
    }

    status = activeTransform.setRotation(rotation);
    if (!status) {
        MGlobal::displayError("Error setting rotation: " + status.errorString());
        return;
    }
}

