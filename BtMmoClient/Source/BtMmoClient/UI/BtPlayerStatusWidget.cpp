#include "BtPlayerStatusWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

void UBtPlayerStatusWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
	WidgetTree->RootWidget = Canvas;
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.85f));
	Panel->SetPadding(FMargin(16));
	UCanvasPanelSlot* PanelSlot = Canvas->AddChildToCanvas(Panel);
	PanelSlot->SetPosition(FVector2D(24, 24));
	PanelSlot->SetSize(FVector2D(280, 112));
	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Content);
	HealthText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HealthText"));
	Content->AddChildToVerticalBox(HealthText);
	HealthBar = WidgetTree->ConstructWidget<UProgressBar>();
	HealthBar->SetFillColorAndOpacity(FLinearColor(0.15f, 0.8f, 0.3f));
	Content->AddChildToVerticalBox(HealthBar)->SetPadding(FMargin(0, 8, 0, 8));
	DeathMessage = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DeathMessage"));
	DeathMessage->SetText(NSLOCTEXT("BtPlayerStatus", "Dead", "사망했습니다"));
	DeathMessage->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.25f, 0.2f)));
	DeathMessage->SetVisibility(ESlateVisibility::Collapsed);
	Content->AddChildToVerticalBox(DeathMessage);
}

void UBtPlayerStatusWidget::SetHealth(int32 Health, int32 MaxHealth)
{
	if (!HealthText || !HealthBar || !DeathMessage) return;
	HealthText->SetText(FText::Format(NSLOCTEXT("BtPlayerStatus", "Health", "내 체력 {0} / {1}"),
		FText::AsNumber(Health), FText::AsNumber(MaxHealth)));
	HealthBar->SetPercent(MaxHealth > 0 ? static_cast<float>(Health) / MaxHealth : 0.0f);
	DeathMessage->SetVisibility(Health <= 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
