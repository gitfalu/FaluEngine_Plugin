/*****************************************************************//**
 * \file   HighPerformanceGpu.cpp
 * \brief  高性能GPUで起動してほしいことを伝えるためのエクスポート変数
 * 
 * \author tsunn
 * \date   October 2026
 *********************************************************************/

#ifdef _WIN32
extern "C"
{
	__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

#endif // _WIN32
