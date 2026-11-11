#pragma once
#ifdef THEATRE_HOST_TEST
#include <extra_pwm_backend.h>
#else
#include <driver/mcpwm_prelude.h>
// Four independent hardware PWM outputs. No timers/ISRs bit-bang GPIOs.
class ExtraPwm {
 mcpwm_timer_handle_t timer=nullptr;mcpwm_oper_handle_t operators[2]{};
 mcpwm_cmpr_handle_t compare[4]{};mcpwm_gen_handle_t generators[4]{};bool ready=false;
 static constexpr uint32_t TICKS=3200;
public:
 bool begin(const uint8_t *pins,uint8_t inactiveHigh=0){
  mcpwm_timer_config_t cfg{};cfg.group_id=0;cfg.clk_src=MCPWM_TIMER_CLK_SRC_DEFAULT;cfg.resolution_hz=80000000;cfg.period_ticks=TICKS;cfg.count_mode=MCPWM_TIMER_COUNT_MODE_UP;
  if(mcpwm_new_timer(&cfg,&timer)!=ESP_OK)return false;
  for(int op=0;op<2;op++){
   mcpwm_operator_config_t oc{};oc.group_id=0;if(mcpwm_new_operator(&oc,&operators[op])!=ESP_OK||mcpwm_operator_connect_timer(operators[op],timer)!=ESP_OK)return false;
   for(int n=0;n<2;n++){int i=op*2+n;mcpwm_comparator_config_t cc{};cc.flags.update_cmp_on_tez=true;
    if(mcpwm_new_comparator(operators[op],&cc,&compare[i])!=ESP_OK)return false;
    mcpwm_generator_config_t gc{};gc.gen_gpio_num=pins[i];if(mcpwm_new_generator(operators[op],&gc,&generators[i])!=ESP_OK)return false;
    if(mcpwm_generator_set_force_level(generators[i],(inactiveHigh&(1<<i))?1:0,true)!=ESP_OK||mcpwm_comparator_set_compare_value(compare[i],1)!=ESP_OK)return false;
    if(mcpwm_generator_set_action_on_timer_event(generators[i],MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,MCPWM_TIMER_EVENT_EMPTY,MCPWM_GEN_ACTION_HIGH))!=ESP_OK)return false;
    if(mcpwm_generator_set_action_on_compare_event(generators[i],MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP,compare[i],MCPWM_GEN_ACTION_LOW))!=ESP_OK)return false;
   }
  }
  ready=mcpwm_timer_enable(timer)==ESP_OK&&mcpwm_timer_start_stop(timer,MCPWM_TIMER_START_NO_STOP)==ESP_OK;return ready;
 }
 bool write(uint8_t index,uint8_t value){
  if(!ready||index>=4)return false;
  if(value==0||value==255)return mcpwm_generator_set_force_level(generators[index],value?1:0,true)==ESP_OK;
  return mcpwm_comparator_set_compare_value(compare[index],(value*TICKS+128)/256)==ESP_OK&&mcpwm_generator_set_force_level(generators[index],-1,true)==ESP_OK;
 }
};
#endif
