// This file is part of Projecteur - https://github.com/jahnf/projecteur - See LICENSE.md and README.md
#include "inputseqedit.h"

#include "deviceinput.h"

// -------------------------------------------------------------------------------------------------
InputSeqEdit::InputSeqEdit(InputMapper* im, QWidget* parent)
  : QWidget(parent)
{
  setInputMapper(im);
}

// -------------------------------------------------------------------------------------------------
void InputSeqEdit::setInputMapper(InputMapper* im)
{
  if (m_inputMapper == im) return;

  auto removeIm = [this](){
    if (m_inputMapper) {
      m_inputMapper->disconnect(this);
      this->disconnect(m_inputMapper);
    }
    m_inputMapper = nullptr;
  };

  removeIm();
  m_inputMapper = im;
  if (m_inputMapper == nullptr) return;

  connect(m_inputMapper, &InputMapper::destroyed, this,
  [removeIm=std::move(removeIm)](){
    removeIm();
  });

  // TODO connect signals from/to
  connect(m_inputMapper, &InputMapper::recordingStarted, this, [](){
    //qDebug() << "Recording started...";
  });
  connect(m_inputMapper, &InputMapper::recordingFinished, this, [](){
    //qDebug() << "Recording finished...";
  });
  connect(m_inputMapper, &InputMapper::recordingModeChanged, this, [](bool /*r*/){
    //qDebug() << "Recording mode... " << r;
  });
  connect(m_inputMapper, &InputMapper::keyEventRecorded, this, [](const KeyEvent& /*ke*/){
    //qDebug() << "Recorded... " << ke;
  });

}
