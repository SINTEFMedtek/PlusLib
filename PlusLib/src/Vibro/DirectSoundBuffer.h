#pragma once

#include <windows.h>
#include <mmsystem.h>
#include "dsound.h"

#include "Wave.h"
#include "DirectSoundInstance.h"

namespace VibroLib
{
	namespace AudioCard
	{
		class VTK_EXPORT DirectSoundBuffer
		{
		public:
			DirectSoundBuffer(void);
			~DirectSoundBuffer(void);
			HRESULT Initialize(DirectSoundInstance* pDSHandle, Wave& signal, DWORD dwFlags = 0, bool primary_buffer = false);
			
			LPDIRECTSOUNDBUFFER operator->() {return pBuffer;}
		private:
			
			HRESULT InitializeSecondaryBuffer(DirectSoundInstance* pDSHandle, Wave& signal, DWORD dwFlags);
			HRESULT InitializePrimaryBuffer(DirectSoundInstance* pDSHandle, Wave& signal, DWORD dwFlags = DSBCAPS_PRIMARYBUFFER);

			LPDIRECTSOUNDBUFFER pBuffer;
			DirectSoundInstance* pDSInstance;
		};
	}
}
