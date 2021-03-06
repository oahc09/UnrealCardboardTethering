#include "CardboardTetheringPrivatePCH.h"
#include "CardboardTethering.h"

#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "PostProcess/PostProcessHMD.h"

#if PLATFORM_WINDOWS

FCardboardTethering::D3D11Bridge::D3D11Bridge(FCardboardTethering* plugin)
  : BridgeBaseImpl(plugin), RenderTargetTexture(nullptr) {}

void FCardboardTethering::D3D11Bridge::BeginRendering() {
  check(IsInRenderingThread());

  static bool Inited = false;
  if (!Inited) {
    Inited = true;
  }
}

void FCardboardTethering::D3D11Bridge::FinishRendering() {
  FScopeLock lock(&Plugin->ActiveUsbDeviceMutex);
  if (Plugin->ActiveUsbDevice.IsValid() && Plugin->ActiveUsbDevice->isSending()) {
    Plugin->ActiveUsbDevice->sendImage(RenderTargetTexture);
  }
}

void FCardboardTethering::D3D11Bridge::Reset() {}

void FCardboardTethering::D3D11Bridge::UpdateViewport(const FViewport& Viewport, FRHIViewport* InViewportRHI) {
  check(IsInGameThread());
  check(InViewportRHI);

  const FTexture2DRHIRef& RT = Viewport.GetRenderTargetTexture();
  check(IsValidRef(RT));

  if (RenderTargetTexture != nullptr) {
    RenderTargetTexture->Release();	//@todo steamvr: need to also release in reset
  }

  RenderTargetTexture = (ID3D11Texture2D*)RT->GetNativeResource();
  RenderTargetTexture->AddRef();

  InViewportRHI->SetCustomPresent(this);
}


void FCardboardTethering::D3D11Bridge::OnBackBufferResize() {}

bool FCardboardTethering::D3D11Bridge::Present(int& SyncInterval) {
  check(IsInRenderingThread());

  FinishRendering();

  return true;
}

void FCardboardTethering::RenderTexture_RenderThread(FRHICommandListImmediate& RHICmdList, FTexture2DRHIParamRef BackBuffer, FTexture2DRHIParamRef SrcTexture) const {
  check(IsInRenderingThread());

  if (WindowMirrorMode == 0) {
    return;
  }

  const uint32 ViewportWidth = BackBuffer->GetSizeX();
  const uint32 ViewportHeight = BackBuffer->GetSizeY();

  SetRenderTarget(RHICmdList, BackBuffer, FTextureRHIRef());
  RHICmdList.SetViewport(0, 0, 0, ViewportWidth, ViewportHeight, 1.0f);

  RHICmdList.SetBlendState(TStaticBlendState<>::GetRHI());
  RHICmdList.SetRasterizerState(TStaticRasterizerState<>::GetRHI());
  RHICmdList.SetDepthStencilState(TStaticDepthStencilState<false, CF_Always>::GetRHI());

  const auto FeatureLevel = GMaxRHIFeatureLevel;
  auto ShaderMap = GetGlobalShaderMap(FeatureLevel);

  TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
  TShaderMapRef<FScreenPS> PixelShader(ShaderMap);

  static FGlobalBoundShaderState BoundShaderState;
  SetGlobalBoundShaderState(RHICmdList, FeatureLevel, BoundShaderState, RendererModule->GetFilterVertexDeclaration().VertexDeclarationRHI, *VertexShader, *PixelShader);

  PixelShader->SetParameters(RHICmdList, TStaticSamplerState<SF_Bilinear>::GetRHI(), SrcTexture);

  if (WindowMirrorMode == 1) {
    // need to clear when rendering only one eye since the borders won't be touched by the DrawRect below
    RHICmdList.Clear(true, FLinearColor::Black, false, 0, false, 0, FIntRect());

    RendererModule->DrawRectangle(
      RHICmdList,
      ViewportWidth / 4, 0,
      ViewportWidth / 2, ViewportHeight,
      0.1f, 0.2f,
      0.3f, 0.6f,
      FIntPoint(ViewportWidth, ViewportHeight),
      FIntPoint(1, 1),
      *VertexShader,
      EDRF_Default);
  } else if (WindowMirrorMode == 2) {
    RendererModule->DrawRectangle(
      RHICmdList,
      0, 0,
      ViewportWidth, ViewportHeight,
      0.0f, 0.0f,
      1.0f, 1.0f,
      FIntPoint(ViewportWidth, ViewportHeight),
      FIntPoint(1, 1),
      *VertexShader,
      EDRF_Default);
  }
}

#endif // PLATFORM_WINDOWS
