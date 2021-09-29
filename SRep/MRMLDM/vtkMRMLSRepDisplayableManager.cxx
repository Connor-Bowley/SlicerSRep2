#include "vtkMRMLSRepDisplayableManager.h"
#include <vtkEventBroker.h>
#include <vtkObjectFactory.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLAbstractViewNode.h>

vtkStandardNewMacro(vtkMRMLSRepDisplayableManager);

//---------------------------------------------------------------------------
void vtkMRMLSRepDisplayableManager::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  os << indent << "SRepDisplayableManager" << std::endl;
}

vtkMRMLSRepDisplayableManager::vtkMRMLSRepDisplayableManager()
    : vtkMRMLAbstractDisplayableManager()
    , SRepNodes()
    , DisplayNodesToWidgets()
    , ObservedSRepNodeEvents({vtkCommand::ModifiedEvent
                            , vtkMRMLTransformableNode::TransformModifiedEvent
                            , vtkMRMLDisplayableNode::DisplayModifiedEvent
    })
{}
vtkMRMLSRepDisplayableManager::~vtkMRMLSRepDisplayableManager() = default;

void vtkMRMLSRepDisplayableManager::ProcessMRMLNodesEvents(vtkObject *caller, unsigned long event, void *callData) {
  vtkMRMLSRepNode * srepNode = vtkMRMLSRepNode::SafeDownCast(caller);
  if (srepNode) {
    bool renderRequested = false;
    for (int i = 0; i < srepNode->GetNumberOfDisplayNodes(); ++i) {
      vtkMRMLSRepDisplayNode* displayNode = vtkMRMLSRepDisplayNode::SafeDownCast(srepNode->GetNthDisplayNode(i));
      auto wit = this->DisplayNodesToWidgets.find(displayNode);
      if (this->DisplayNodesToWidgets.end() == wit) {
        this->AddDisplayNode(displayNode);
        wit = this->DisplayNodesToWidgets.find(displayNode);
        if (this->DisplayNodesToWidgets.end() == wit) {
          vtkErrorMacro("vtkMRMLSRepDisplayableManager: can't find widget right after adding it");
          continue;
        }
      }
      auto& widget = wit->second;
      widget->UpdateFromMRML(srepNode, event, callData);
      if (widget->GetNeedToRender()) {
        renderRequested = true;
        widget->NeedToRenderOff();
      }
    }

    if (renderRequested) {
      this->RequestRender();
    }
  } else {
    this->Superclass::ProcessMRMLNodesEvents(caller, event, callData);
  }
}

void vtkMRMLSRepDisplayableManager::OnMRMLSceneNodeAdded(vtkMRMLNode* node) {
  if (!node || !this->GetMRMLScene()) {
      return;
  }

    // if the scene is still updating, jump out
  if (this->GetMRMLScene()->IsBatchProcessing()) {
    this->SetUpdateFromMRMLRequested(true);
    return;
  }

  if (node->IsA("vtkMRMLSRepNode")) {
    this->AddSRepNode(vtkMRMLSRepNode::SafeDownCast(node));
    this->RequestRender();
  }
}

void vtkMRMLSRepDisplayableManager::AddSRepNode(vtkMRMLSRepNode* node) {
  if (!node) {
    return;
  }

  vtkMRMLAbstractViewNode* viewNode = vtkMRMLAbstractViewNode::SafeDownCast(this->GetMRMLDisplayableNode());
  if (!viewNode) {
    return;
  }

  this->AddObservations(node);
  this->SRepNodes.insert(node);

  // Add Display Nodes
  const int numDisplayNodes = node->GetNumberOfDisplayNodes();
  for (int i = 0; i < numDisplayNodes; ++i) {
    vtkMRMLSRepDisplayNode *display = vtkMRMLSRepDisplayNode::SafeDownCast(node->GetNthDisplayNode(i));
    if (display
      && display->IsDisplayableInView(viewNode->GetID()))
    {
      this->AddDisplayNode(display);
    }
  }
}

void vtkMRMLSRepDisplayableManager::AddDisplayNode(vtkMRMLSRepDisplayNode* displayNode) {
  if (!displayNode) {
    return;
  }

  //see if we already have a widget for this display node. If so, return
  if (this->DisplayNodesToWidgets.count(displayNode)) {
    return;
  }

  auto newWidget = this->CreateWidget(displayNode);
  if (!newWidget) {
    vtkErrorMacro("vtkMRMLSRepDisplayableManager: Failed to create widget");
    return;
  }

  const auto ret = this->DisplayNodesToWidgets.insert(std::make_pair(displayNode, newWidget));
  if (!ret.second) {
    vtkErrorMacro("vtkMRMLSRepDisplayableManager: Error adding widget to map");
    return;
  }

  newWidget->UpdateFromMRML(displayNode, 0);

  this->RequestRender();
}

vtkSmartPointer<vtkSlicerSRepWidget> vtkMRMLSRepDisplayableManager::CreateWidget(vtkMRMLSRepDisplayNode* srepDisplayNode) {
  vtkMRMLSRepNode* srepNode = srepDisplayNode->GetSRepNode();
  if (!srepNode) {
    vtkErrorMacro("vtkMRMLSRepDisplayableManager: Error cannot create widget without srep node");
    return nullptr;
  }

  vtkMRMLAbstractViewNode* viewNode = vtkMRMLAbstractViewNode::SafeDownCast(this->GetMRMLDisplayableNode());
  vtkRenderer* renderer = this->GetRenderer();

  auto widget = vtkSmartPointer<vtkSlicerSRepWidget>::New();
  widget->SetMRMLApplicationLogic(this->GetMRMLApplicationLogic());
  widget->CreateDefaultRepresentation(srepDisplayNode, viewNode, renderer);
  return widget;
}

//---------------------------------------------------------------------------
void vtkMRMLSRepDisplayableManager::AddObservations(vtkMRMLSRepNode* node)
{
  vtkCallbackCommand* callbackCommand = this->GetMRMLNodesCallbackCommand();
  vtkEventBroker* broker = vtkEventBroker::GetInstance();
  for (auto observedSRepNodeEvent : this->ObservedSRepNodeEvents) {
    if (!broker->GetObservationExist(node, observedSRepNodeEvent, this, callbackCommand)) {
      broker->AddObservation(node, observedSRepNodeEvent, this, callbackCommand);
    }
  }
}

//---------------------------------------------------------------------------
void vtkMRMLSRepDisplayableManager::RemoveObservations(vtkMRMLSRepNode* node)
{
  vtkCallbackCommand* callbackCommand = this->GetMRMLNodesCallbackCommand();
  vtkEventBroker* broker = vtkEventBroker::GetInstance();
  for (auto observedSRepNodeEvent : this->ObservedSRepNodeEvents) {
    vtkEventBroker::ObservationVector observations;
    observations = broker->GetObservations(node, observedSRepNodeEvent, this, callbackCommand);
    broker->RemoveObservations(observations);
  }
}
