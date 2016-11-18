/*
 * Copyright (c) 2005-2013 Imperas Software Ltd., www.imperas.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied.
 *
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */


#ifndef BSP_H_
#define BSP_H_


#define LED()    *((volatile char *) 0x40004000+4)
#define UARTTX() *((volatile char *) 0x1000c000)



void ledWrite(int value);

void uartWrite(char c);

#endif /* BSP_H_ */
