#include "BtMonsterHealthWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

void UBtMonsterHealthWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetVisibility(ESlateVisibility::HitTestInvisible);
	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	WidgetTree->RootWidget = Panel;
	Panel->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.8f));
	Panel->SetPadding(FMargin(6, 4));
	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Content);
	HealthText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("HealthText"));
	HealthText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 14));
	HealthText->SetJustification(ETextJustify::Center);
	Content->AddChildToVerticalBox(HealthText);
	HealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthBar"));
	HealthBar->SetFillColorAndOpacity(FLinearColor(0.85f, 0.15f, 0.12f));
	Content->AddChildToVerticalBox(HealthBar)->SetPadding(FMargin(0, 3, 0, 0));
}

void UBtMonsterHealthWidget::SetHealth(int32 Health, int32 MaxHealth)
{
	if (!HealthText || !HealthBar) return;
	HealthText->SetText(FText::Format(NSLOCTEXT("BtMonsterHealth", "Health", "{0} / {1}"),
		FText::AsNumber(Health), FText::AsNumber(MaxHealth)));
	HealthBar->SetPercent(MaxHealth > 0 ? static_cast<float>(Health) / MaxHealth : 0.0f);
}
